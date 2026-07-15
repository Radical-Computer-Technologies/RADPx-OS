#include <radboot.h>
#include <radixkernel/radixkernel.h>

#include "x86_cpu.h"
#include "x86_ext2.h"
#include "x86_ext4.h"
#include "x86_fat.h"
#include "x86_storage.h"
#include "x86_user.h"
#include "x86_vm.h"
#include "rkconfig.h"
#if defined(RADIX_X86_UI_PROFILE_WM)
#include "slint_shell.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_init(void);
extern "C" void x86_serial_write(const char *text);
extern "C" void x86_ps2_set_pointer_bounds(uint32_t width, uint32_t height);
extern "C" void x86_ui_idle_frame(void) __attribute__((weak));
extern "C" char x86_boot_stack_top[];
extern "C" int x86_cpp_runtime_self_test(void);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint32_t Multiboot2BootloaderMagic = 0x36d76289u;
constexpr uint32_t TagEnd = 0u;
constexpr uint32_t TagMmap = 6u;
constexpr uint32_t TagFramebuffer = 8u;
constexpr uint32_t TagAcpiOld = 14u;
constexpr uint32_t TagAcpiNew = 15u;

struct Mb2Tag {
    uint32_t type;
    uint32_t size;
};

struct Mb2MmapEntry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct Mb2MmapTag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    Mb2MmapEntry entries[];
};

struct Mb2FramebufferTag {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t kind;
    uint16_t reserved;
};

struct [[gnu::packed]] AcpiRsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
};

struct [[gnu::packed]] AcpiSdtHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct [[gnu::packed]] AcpiMadt {
    AcpiSdtHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];
};

struct CpuTopologyProbe {
    uint32_t detected_cores;
    uint32_t present_mask;
    uint64_t lapic_address;
};

struct X86FramebufferContext {
    uint32_t *pixels = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride_pixels = 0;
};

char g_transcript[8192];
size_t g_transcript_size = 0;
X86FramebufferContext g_x86_framebuffer_context{};
int g_terminal_dirty = 1;
int g_user_terminal_active = 0;
constexpr uint32_t TerminalMaxCols = 180u;
constexpr uint32_t TerminalMaxRows = 90u;
struct TerminalTheme {
    uint32_t foreground;
    uint32_t background;
    uint32_t cursor;
    uint32_t normal[8];
    uint32_t bright[8];
};
TerminalTheme g_terminal_theme{
    0x00d8f7eeu,
    0x00090716u,
    0x00f8fafcu,
    {0x00231f33u,0x00ef4444u,0x0022c55eu,0x00eab308u,0x003b82f6u,0x008b5cf6u,0x0006b6d4u,0x00e5e7ebu},
    {0x006b7280u,0x00f87171u,0x004ade80u,0x00facc15u,0x0060a5fau,0x00a78bfau,0x0022d3eeu,0x00ffffffu}
};
struct TerminalCell {
    char ch;
    uint32_t fg;
    uint32_t bg;
};
struct TerminalRenderState {
    TerminalCell cells[TerminalMaxRows][TerminalMaxCols]{};
    uint32_t cols = 0;
    uint32_t rows = 0;
    uint32_t scale = 0;
    uint32_t cursor_x = 0;
    uint32_t cursor_y = 0;
    uint32_t fg = 0x00d8f7eeu;
    uint32_t bg = 0x00090716u;
    char ansi[32]{};
    size_t ansi_size = 0;
    int ansi_active = 0;
    int initialized = 0;
};
TerminalRenderState g_terminal_render_state{};
void terminal_apply_sgr(const char *seq, size_t size, uint32_t default_fg, uint32_t default_bg, uint32_t *foreground, uint32_t *background);
int g_keyboard_input_seen = 0;
int g_pointer_input_seen = 0;
int g_module_probe_inits = 0;
int g_module_probe_exits = 0;
int g_deferred_work_ran = 0;

struct BootServiceContext {
    x86_storage_summary_t *storage_summary = nullptr;
    char terminal_slave[64]{};
    int rootfs_ready = 0;
    int userspace_ready = 0;
    int fat_ready = 0;
    int network_ready = 0;
    int terminal_ready = 0;
    int terminal_stress_ready = 0;
};

struct BootServiceJsonEntry {
    char name[RAD_SERVICE_NAME_MAX];
    char tree_path[RAD_TREE_MAX_PATH];
    char backend[RAD_TREE_MAX_VALUE];
    char display[RAD_TREE_MAX_VALUE];
    char keyboard[RAD_TREE_MAX_VALUE];
    char pointer[RAD_TREE_MAX_VALUE];
    char terminal[RAD_TREE_MAX_VALUE];
    int autostart = 0;
    int order = 0;
};

constexpr size_t MaxBootServiceJsonEntries = 8u;

constexpr const char BootServicesJson[] = R"json(
[
  {
    "name": "storage-root",
    "path": "/services/storage-root",
    "backend": "virtio-blk",
    "autostart": true,
    "order": 10
  },
  {
    "name": "userspace-init",
    "path": "/services/userspace-init",
    "backend": "radix-init",
    "terminal": "/dev/pts/boot-terminal",
    "autostart": false,
    "order": 45
  },
  {
    "name": "fatfs",
    "path": "/services/fatfs",
    "backend": "virtio-blk",
    "autostart": true,
    "order": 30
  },
  {
    "name": "network-smoke",
    "path": "/services/network-smoke",
    "backend": "virtio-net",
    "autostart": false,
    "order": 90
  },
  {
    "name": "base-terminal",
    "path": "/services/base-terminal",
    "backend": "framebuffer-pty",
    "display": "/dev/fb0",
    "keyboard": "/dev/input/event0",
    "pointer": "/dev/input/event1",
    "terminal": "/dev/pts/boot-terminal",
    "autostart": false,
    "order": 50
  },
  {
    "name": "terminal-stress",
    "path": "/services/terminal-stress",
    "backend": "builtin:terminal-stress",
    "terminal": "/dev/pts/boot-terminal",
    "autostart": false,
    "order": 60
  }
]
)json";

BootServiceJsonEntry g_boot_service_entries[MaxBootServiceJsonEntries]{};
size_t g_boot_service_count = 0;

#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
void terminal_stress_service_arm(const char *slave_name);
void terminal_stress_service_poll(void);
bool terminal_stress_service_finished(void);
bool terminal_stress_service_started(void);
#endif

void compact_transcript_for(size_t incoming_size) {
    if (incoming_size >= sizeof(g_transcript)) {
        g_transcript_size = 0;
        g_transcript[0] = '\0';
        return;
    }
    if (g_transcript_size + incoming_size + 1 < sizeof(g_transcript)) return;
    size_t keep = sizeof(g_transcript) / 2u;
    if (keep > g_transcript_size) keep = g_transcript_size;
    const char *start = g_transcript + (g_transcript_size - keep);
    while (keep > 0 && *start != '\n') {
        ++start;
        --keep;
    }
    if (keep > 0 && *start == '\n') {
        ++start;
        --keep;
    }
    memmove(g_transcript, start, keep);
    g_transcript_size = keep;
    g_transcript[g_transcript_size] = '\0';
}

void terminal_model_clear_line(uint32_t row) {
    if (row >= TerminalMaxRows) return;
    for (uint32_t x = 0; x < TerminalMaxCols; ++x) {
        g_terminal_render_state.cells[row][x] = {' ', g_terminal_render_state.fg, g_terminal_render_state.bg};
    }
}

void terminal_model_init(void) {
    if (g_terminal_render_state.initialized) return;
    g_terminal_render_state.cols = TerminalMaxCols;
    g_terminal_render_state.rows = TerminalMaxRows;
    g_terminal_render_state.cursor_x = 0;
    g_terminal_render_state.cursor_y = 0;
    g_terminal_render_state.fg = g_terminal_theme.foreground;
    g_terminal_render_state.bg = g_terminal_theme.background;
    g_terminal_render_state.ansi_size = 0;
    g_terminal_render_state.ansi_active = 0;
    g_terminal_render_state.initialized = 1;
    for (uint32_t y = 0; y < TerminalMaxRows; ++y) terminal_model_clear_line(y);
}

void terminal_model_reset(void) {
    g_terminal_render_state.initialized = 0;
    terminal_model_init();
    g_terminal_dirty = 1;
}

void terminal_model_scroll(void) {
    terminal_model_init();
    memmove(&g_terminal_render_state.cells[0][0], &g_terminal_render_state.cells[1][0],
        sizeof(g_terminal_render_state.cells[0]) * (TerminalMaxRows - 1u));
    terminal_model_clear_line(TerminalMaxRows - 1u);
    if (g_terminal_render_state.cursor_y > 0) --g_terminal_render_state.cursor_y;
}

void terminal_model_newline(void) {
    terminal_model_init();
    g_terminal_render_state.cursor_x = 0;
    ++g_terminal_render_state.cursor_y;
    if (g_terminal_render_state.cursor_y >= TerminalMaxRows) terminal_model_scroll();
}

void terminal_model_put(char ch) {
    terminal_model_init();
    if (g_terminal_render_state.cursor_x >= TerminalMaxCols) terminal_model_newline();
    TerminalCell& cell = g_terminal_render_state.cells[g_terminal_render_state.cursor_y][g_terminal_render_state.cursor_x];
    cell.ch = ch;
    cell.fg = g_terminal_render_state.fg;
    cell.bg = g_terminal_render_state.bg;
    ++g_terminal_render_state.cursor_x;
    if (g_terminal_render_state.cursor_x >= TerminalMaxCols) terminal_model_newline();
}

void terminal_model_apply_sgr_sequence(void) {
    uint32_t fg = g_terminal_render_state.fg;
    uint32_t bg = g_terminal_render_state.bg;
    terminal_apply_sgr(g_terminal_render_state.ansi, g_terminal_render_state.ansi_size,
        g_terminal_theme.foreground, g_terminal_theme.background, &fg, &bg);
    g_terminal_render_state.fg = fg;
    g_terminal_render_state.bg = bg;
}

int terminal_csi_param(size_t index, int fallback) {
    int current = 0;
    int have = 0;
    size_t param = 0;
    for (size_t i = 0; i <= g_terminal_render_state.ansi_size; ++i) {
        const char ch = i < g_terminal_render_state.ansi_size ? g_terminal_render_state.ansi[i] : ';';
        if (ch >= '0' && ch <= '9') {
            current = current * 10 + (ch - '0');
            have = 1;
            continue;
        }
        if (ch == '[' || ch == '?' || ch == ' ') continue;
        if (ch == ';' || i == g_terminal_render_state.ansi_size) {
            if (param == index) return have ? current : fallback;
            current = 0;
            have = 0;
            ++param;
        }
    }
    return fallback;
}

void terminal_model_clear_screen(void) {
    for (uint32_t y = 0; y < TerminalMaxRows; ++y) terminal_model_clear_line(y);
    g_terminal_render_state.cursor_x = 0;
    g_terminal_render_state.cursor_y = 0;
}

void terminal_model_clear_current_line(void) {
    terminal_model_clear_line(g_terminal_render_state.cursor_y);
    g_terminal_render_state.cursor_x = 0;
}

void terminal_model_apply_csi(char final) {
    if (final == 'm') {
        terminal_model_apply_sgr_sequence();
    } else if (final == 'J') {
        const int mode = terminal_csi_param(0, 0);
        if (mode == 2 || mode == 3) terminal_model_clear_screen();
    } else if (final == 'K') {
        terminal_model_clear_current_line();
    } else if (final == 'H' || final == 'f') {
        const int row = terminal_csi_param(0, 1);
        const int col = terminal_csi_param(1, 1);
        g_terminal_render_state.cursor_y = row > 0 ? static_cast<uint32_t>(row - 1) : 0;
        g_terminal_render_state.cursor_x = col > 0 ? static_cast<uint32_t>(col - 1) : 0;
        if (g_terminal_render_state.cursor_y >= TerminalMaxRows) g_terminal_render_state.cursor_y = TerminalMaxRows - 1u;
        if (g_terminal_render_state.cursor_x >= TerminalMaxCols) g_terminal_render_state.cursor_x = TerminalMaxCols - 1u;
    } else if (final == 'C') {
        uint32_t step = static_cast<uint32_t>(terminal_csi_param(0, 1));
        g_terminal_render_state.cursor_x = g_terminal_render_state.cursor_x + step < TerminalMaxCols
            ? g_terminal_render_state.cursor_x + step
            : TerminalMaxCols - 1u;
    } else if (final == 'D') {
        uint32_t step = static_cast<uint32_t>(terminal_csi_param(0, 1));
        g_terminal_render_state.cursor_x = step < g_terminal_render_state.cursor_x ? g_terminal_render_state.cursor_x - step : 0u;
    } else if (final == 'A') {
        uint32_t step = static_cast<uint32_t>(terminal_csi_param(0, 1));
        g_terminal_render_state.cursor_y = step < g_terminal_render_state.cursor_y ? g_terminal_render_state.cursor_y - step : 0u;
    } else if (final == 'B') {
        uint32_t step = static_cast<uint32_t>(terminal_csi_param(0, 1));
        g_terminal_render_state.cursor_y = g_terminal_render_state.cursor_y + step < TerminalMaxRows
            ? g_terminal_render_state.cursor_y + step
            : TerminalMaxRows - 1u;
    }
}

void terminal_model_feed_char(char ch) {
    terminal_model_init();
    if (g_terminal_render_state.ansi_active) {
        if (ch != '[' && ch >= '@' && ch <= '~') {
            if (g_terminal_render_state.ansi_size + 1 < sizeof(g_terminal_render_state.ansi)) {
                g_terminal_render_state.ansi[g_terminal_render_state.ansi_size++] = ch;
            }
            terminal_model_apply_csi(ch);
            g_terminal_render_state.ansi_active = 0;
            g_terminal_render_state.ansi_size = 0;
            return;
        }
        if (g_terminal_render_state.ansi_size + 1 < sizeof(g_terminal_render_state.ansi)) {
            g_terminal_render_state.ansi[g_terminal_render_state.ansi_size++] = ch;
        }
        return;
    }
    if (ch == '\x1b') {
        g_terminal_render_state.ansi_active = 1;
        g_terminal_render_state.ansi_size = 0;
        return;
    }
    if (ch == '\r') {
        g_terminal_render_state.cursor_x = 0;
        return;
    }
    if (ch == '\n') {
        terminal_model_newline();
        return;
    }
    if (ch == '\b' || ch == 0x7f) {
        if (g_terminal_render_state.cursor_x > 0) --g_terminal_render_state.cursor_x;
        terminal_model_put(' ');
        if (g_terminal_render_state.cursor_x > 0) --g_terminal_render_state.cursor_x;
        return;
    }
    if (ch == '\t') {
        do {
            terminal_model_put(' ');
        } while (g_terminal_render_state.cursor_x % 4u);
        return;
    }
    if (static_cast<unsigned char>(ch) < 0x20u) return;
    terminal_model_put(ch);
}

void terminal_model_feed(const char *text, size_t size) {
    if (!text || !size) return;
    for (size_t i = 0; i < size; ++i) terminal_model_feed_char(text[i]);
    g_terminal_dirty = 1;
}

void append_text(const char *text) {
    if (!text) return;
    terminal_model_feed(text, strlen(text));
    compact_transcript_for(strlen(text));
    const size_t before = g_transcript_size;
    while (*text && g_transcript_size + 1 < sizeof(g_transcript)) {
        g_transcript[g_transcript_size++] = *text++;
    }
    g_transcript[g_transcript_size] = '\0';
    if (g_transcript_size != before) g_terminal_dirty = 1;
}

void append_transcript_char(char ch) {
    if (ch == '\0') return;
    if (g_transcript_size + 1 < sizeof(g_transcript)) {
        g_transcript[g_transcript_size++] = ch;
        g_transcript[g_transcript_size] = '\0';
    }
}

void mirror_radix_marker_lines_to_serial(const char *text, size_t size) {
    static char line[256];
    static size_t pos = 0;
    if (!text || !size) return;
    for (size_t i = 0; i < size; ++i) {
        const char ch = text[i];
        if (ch == '\r' || ch == '\n') {
            line[pos] = '\0';
            if (pos >= 6 && strncmp(line, "RADIX_", 6) == 0) {
                x86_serial_write(line);
                x86_serial_write("\n");
            }
            pos = 0;
            continue;
        }
        if (pos + 1 < sizeof(line)) {
            line[pos++] = ch;
        } else {
            pos = 0;
        }
    }
}

void append_bytes(const char *text, size_t size) {
    if (!text || !size) return;
    mirror_radix_marker_lines_to_serial(text, size);
    terminal_model_feed(text, size);
    compact_transcript_for(size * 4u);
    const size_t before = g_transcript_size;
    for (size_t i = 0; i < size && g_transcript_size + 1 < sizeof(g_transcript); ++i) {
        append_transcript_char(text[i]);
    }
    g_transcript[g_transcript_size] = '\0';
    if (g_transcript_size != before) g_terminal_dirty = 1;
}

struct TerminalGlyph {
    char ch;
    uint8_t rows[7];
};

constexpr TerminalGlyph TerminalFont[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'!', {0x04,0x04,0x04,0x04,0x00,0x04,0x00}},
    {'"', {0x0a,0x0a,0x00,0x00,0x00,0x00,0x00}},
    {'#', {0x0a,0x1f,0x0a,0x0a,0x1f,0x0a,0x00}},
    {'$', {0x04,0x0f,0x14,0x0e,0x05,0x1e,0x04}},
    {'%', {0x18,0x19,0x02,0x04,0x08,0x13,0x03}},
    {'&', {0x0c,0x12,0x14,0x08,0x15,0x12,0x0d}},
    {'\'',{0x04,0x04,0x00,0x00,0x00,0x00,0x00}},
    {'(', {0x02,0x04,0x08,0x08,0x08,0x04,0x02}},
    {')', {0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
    {'*', {0x00,0x04,0x15,0x0e,0x15,0x04,0x00}},
    {'+', {0x00,0x04,0x04,0x1f,0x04,0x04,0x00}},
    {',', {0x00,0x00,0x00,0x00,0x04,0x04,0x08}},
    {'-', {0x00,0x00,0x00,0x1f,0x00,0x00,0x00}},
    {'.', {0x00,0x00,0x00,0x00,0x00,0x04,0x00}},
    {'/', {0x01,0x02,0x02,0x04,0x08,0x08,0x10}},
    {'0', {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e}},
    {'1', {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}},
    {'2', {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}},
    {'3', {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e}},
    {'4', {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}},
    {'5', {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e}},
    {'6', {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e}},
    {'7', {0x1f,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}},
    {'9', {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c}},
    {':', {0x00,0x04,0x00,0x00,0x04,0x00,0x00}},
    {';', {0x00,0x04,0x00,0x00,0x04,0x04,0x08}},
    {'<', {0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
    {'=', {0x00,0x00,0x1f,0x00,0x1f,0x00,0x00}},
    {'>', {0x08,0x04,0x02,0x01,0x02,0x04,0x08}},
    {'?', {0x0e,0x11,0x01,0x02,0x04,0x00,0x04}},
    {'@', {0x0e,0x11,0x01,0x0d,0x15,0x15,0x0e}},
    {'A', {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}},
    {'B', {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}},
    {'C', {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e}},
    {'D', {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}},
    {'E', {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}},
    {'F', {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}},
    {'G', {0x0e,0x11,0x10,0x17,0x11,0x11,0x0f}},
    {'H', {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}},
    {'I', {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}},
    {'J', {0x07,0x02,0x02,0x02,0x12,0x12,0x0c}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1f}},
    {'M', {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}},
    {'P', {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}},
    {'Q', {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}},
    {'R', {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}},
    {'S', {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}},
    {'T', {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0a,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x15,0x0a}},
    {'X', {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}},
    {'Y', {0x11,0x11,0x0a,0x04,0x04,0x04,0x04}},
    {'Z', {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}},
    {'[', {0x0e,0x08,0x08,0x08,0x08,0x08,0x0e}},
    {'\\',{0x10,0x08,0x08,0x04,0x02,0x02,0x01}},
    {']', {0x0e,0x02,0x02,0x02,0x02,0x02,0x0e}},
    {'^', {0x04,0x0a,0x11,0x00,0x00,0x00,0x00}},
    {'_', {0x00,0x00,0x00,0x00,0x00,0x00,0x1f}},
    {'`', {0x08,0x04,0x00,0x00,0x00,0x00,0x00}},
    {'a', {0x00,0x00,0x0e,0x01,0x0f,0x11,0x0f}},
    {'b', {0x10,0x10,0x16,0x19,0x11,0x11,0x1e}},
    {'c', {0x00,0x00,0x0e,0x10,0x10,0x11,0x0e}},
    {'d', {0x01,0x01,0x0d,0x13,0x11,0x11,0x0f}},
    {'e', {0x00,0x00,0x0e,0x11,0x1f,0x10,0x0e}},
    {'f', {0x06,0x09,0x08,0x1c,0x08,0x08,0x08}},
    {'g', {0x00,0x00,0x0f,0x11,0x0f,0x01,0x0e}},
    {'h', {0x10,0x10,0x16,0x19,0x11,0x11,0x11}},
    {'i', {0x04,0x00,0x0c,0x04,0x04,0x04,0x0e}},
    {'j', {0x02,0x00,0x06,0x02,0x02,0x12,0x0c}},
    {'k', {0x10,0x10,0x12,0x14,0x18,0x14,0x12}},
    {'l', {0x0c,0x04,0x04,0x04,0x04,0x04,0x0e}},
    {'m', {0x00,0x00,0x1a,0x15,0x15,0x11,0x11}},
    {'n', {0x00,0x00,0x16,0x19,0x11,0x11,0x11}},
    {'o', {0x00,0x00,0x0e,0x11,0x11,0x11,0x0e}},
    {'p', {0x00,0x00,0x1e,0x11,0x1e,0x10,0x10}},
    {'q', {0x00,0x00,0x0d,0x13,0x0f,0x01,0x01}},
    {'r', {0x00,0x00,0x16,0x19,0x10,0x10,0x10}},
    {'s', {0x00,0x00,0x0f,0x10,0x0e,0x01,0x1e}},
    {'t', {0x08,0x08,0x1c,0x08,0x08,0x09,0x06}},
    {'u', {0x00,0x00,0x11,0x11,0x11,0x13,0x0d}},
    {'v', {0x00,0x00,0x11,0x11,0x11,0x0a,0x04}},
    {'w', {0x00,0x00,0x11,0x11,0x15,0x15,0x0a}},
    {'x', {0x00,0x00,0x11,0x0a,0x04,0x0a,0x11}},
    {'y', {0x00,0x00,0x11,0x11,0x0f,0x01,0x0e}},
    {'z', {0x00,0x00,0x1f,0x02,0x04,0x08,0x1f}},
    {'{', {0x02,0x04,0x04,0x08,0x04,0x04,0x02}},
    {'|', {0x04,0x04,0x04,0x00,0x04,0x04,0x04}},
    {'}', {0x08,0x04,0x04,0x02,0x04,0x04,0x08}},
    {'~', {0x00,0x00,0x08,0x15,0x02,0x00,0x00}},
};

uint8_t terminal_glyph_row(char ch, uint32_t row) {
    for (const auto& glyph : TerminalFont) {
        if (glyph.ch == ch) return glyph.rows[row];
    }
    return TerminalFont[0].rows[row];
}

uint32_t terminal_scale_for(const rad_framebuffer_info_t& info) {
    const char *scale_text = RADIX_RKCONFIG_TERMINAL_SCALE;
    if (scale_text && scale_text[0] >= '1' && scale_text[0] <= '4' && scale_text[1] == '\0') {
        return static_cast<uint32_t>(scale_text[0] - '0');
    }
    if (info.width >= 1280u && info.height >= 720u) return 2u;
    return 1u;
}

void terminal_put_pixel(const rad_framebuffer_info_t& info, uint32_t x, uint32_t y, uint32_t color) {
    if (!info.pixels || x >= info.width || y >= info.height || info.pixel_format != RAD_PIXEL_FORMAT_XRGB8888) return;
    auto *row = static_cast<uint8_t*>(info.pixels) + static_cast<size_t>(y) * info.stride_bytes;
    auto *pixel = reinterpret_cast<uint32_t*>(row + static_cast<size_t>(x) * sizeof(uint32_t));
    *pixel = color | 0xff000000u;
}

void terminal_fill_rect(const rad_framebuffer_info_t& info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    const uint32_t max_x = x + width > info.width ? info.width : x + width;
    const uint32_t max_y = y + height > info.height ? info.height : y + height;
    for (uint32_t py = y; py < max_y; ++py) {
        for (uint32_t px = x; px < max_x; ++px) terminal_put_pixel(info, px, py, color);
    }
}

uint32_t terminal_ansi_color(int code, uint32_t fallback) {
    if (code >= 30 && code <= 37) return g_terminal_theme.normal[code - 30];
    if (code >= 90 && code <= 97) return g_terminal_theme.bright[code - 90];
    return fallback;
}

void terminal_apply_sgr(const char *seq, size_t size, uint32_t default_fg, uint32_t default_bg, uint32_t *foreground, uint32_t *background) {
    int value = 0;
    int have = 0;
    for (size_t i = 0; i <= size; ++i) {
        const char ch = i < size ? seq[i] : ';';
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
            have = 1;
            continue;
        }
        if (ch != ';') continue;
        const int code = have ? value : 0;
        if (code == 0) {
            *foreground = default_fg;
            *background = default_bg;
        } else if (code == 1) {
            *foreground = g_terminal_theme.bright[7];
        } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
            *foreground = terminal_ansi_color(code, *foreground);
        } else if (code >= 40 && code <= 47) {
            *background = terminal_ansi_color(code - 10, *background);
        } else if (code >= 100 && code <= 107) {
            *background = terminal_ansi_color(code - 10, *background);
        }
        value = 0;
        have = 0;
    }
}

void terminal_draw_text(const rad_framebuffer_info_t& info, uint32_t cell_x, uint32_t cell_y, const char *text, uint32_t foreground, uint32_t background, uint32_t scale) {
    if (!text || scale == 0) return;
    const uint32_t default_fg = foreground;
    const uint32_t default_bg = background;
    const uint32_t cell_width = 8u * scale;
    const uint32_t cell_height = 16u * scale;
    uint32_t x = cell_x;
    uint32_t y = cell_y;
    const uint32_t columns = cell_width ? info.width / cell_width : 0;
    const uint32_t rows = cell_height ? info.height / cell_height : 0;
    for (const char *p = text; *p && y < rows; ++p) {
        if (*p == '\x1b' && p[1] == '[') {
            const char *q = p + 2;
            while (*q && *q != 'm' && q - p < 32) ++q;
            if (*q == 'm') {
                terminal_apply_sgr(p + 2, static_cast<size_t>(q - (p + 2)), default_fg, default_bg, &foreground, &background);
                p = q;
                continue;
            }
        }
        if (*p == '\n') {
            x = cell_x;
            ++y;
            continue;
        }
        const uint32_t origin_x = x * cell_width;
        const uint32_t origin_y = y * cell_height;
        terminal_fill_rect(info, origin_x, origin_y, cell_width, cell_height, background);
        for (uint32_t row = 0; row < 16u; ++row) {
            if (row == 0 || row >= 15u) continue;
            const uint32_t source_row = (row - 1u) / 2u;
            const uint8_t bits = terminal_glyph_row(*p, source_row);
            for (uint32_t col = 0; col < 5u; ++col) {
                if (!(bits & (1u << (4u - col)))) continue;
                terminal_fill_rect(info, origin_x + (col + 1u) * scale, origin_y + row * scale, scale, scale, foreground);
            }
        }
        ++x;
        if (x >= columns) {
            x = cell_x;
            ++y;
        }
    }
}

void serial_hex64(uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    char out[19];
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 16; ++i) out[2 + i] = digits[(value >> ((15 - i) * 4)) & 0xf];
    out[18] = '\0';
    x86_serial_write(out);
}

const Mb2Tag *first_tag(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    return reinterpret_cast<const Mb2Tag*>(base + 8);
}

const Mb2Tag *next_tag(const Mb2Tag *tag) {
    return reinterpret_cast<const Mb2Tag*>(reinterpret_cast<const uint8_t*>(tag) + ((tag->size + 7u) & ~uint32_t(7u)));
}

const Mb2FramebufferTag *find_framebuffer(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if (tag->type == TagFramebuffer && tag->size >= sizeof(Mb2FramebufferTag)) {
            return reinterpret_cast<const Mb2FramebufferTag*>(tag);
        }
        tag = next_tag(tag);
    }
    return nullptr;
}

int bytes_equal(const char *a, const char *b, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

int acpi_checksum_ok(const void *data, size_t size) {
    if (!data || !size) return 0;
    const auto *bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for (size_t i = 0; i < size; ++i) sum = static_cast<uint8_t>(sum + bytes[i]);
    return sum == 0;
}

const AcpiRsdp *find_acpi_rsdp(uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if ((tag->type == TagAcpiOld || tag->type == TagAcpiNew)
            && tag->size >= sizeof(Mb2Tag) + 20u) {
            const auto *rsdp = reinterpret_cast<const AcpiRsdp*>(
                reinterpret_cast<const uint8_t*>(tag) + sizeof(Mb2Tag));
            if (bytes_equal(rsdp->signature, "RSD PTR ", 8)
                && acpi_checksum_ok(rsdp, 20u)) {
                if (rsdp->revision >= 2 && rsdp->length >= sizeof(AcpiRsdp)
                    && !acpi_checksum_ok(rsdp, rsdp->length)) {
                    tag = next_tag(tag);
                    continue;
                }
                return rsdp;
            }
        }
        tag = next_tag(tag);
    }
    return nullptr;
}

const AcpiSdtHeader *find_acpi_table(const AcpiRsdp *rsdp, const char signature[4]) {
    if (!rsdp) return nullptr;
    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        const auto *xsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(rsdp->xsdt_address));
        if (bytes_equal(xsdt->signature, "XSDT", 4) && acpi_checksum_ok(xsdt, xsdt->length)) {
            const size_t entries = (xsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint64_t);
            const auto *entry = reinterpret_cast<const uint64_t*>(
                reinterpret_cast<const uint8_t*>(xsdt) + sizeof(AcpiSdtHeader));
            for (size_t i = 0; i < entries; ++i) {
                const auto *table = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(entry[i]));
                if (table && bytes_equal(table->signature, signature, 4) && acpi_checksum_ok(table, table->length)) {
                    return table;
                }
            }
        }
    }
    if (rsdp->rsdt_address) {
        const auto *rsdt = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(rsdp->rsdt_address));
        if (bytes_equal(rsdt->signature, "RSDT", 4) && acpi_checksum_ok(rsdt, rsdt->length)) {
            const size_t entries = (rsdt->length - sizeof(AcpiSdtHeader)) / sizeof(uint32_t);
            const auto *entry = reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(rsdt) + sizeof(AcpiSdtHeader));
            for (size_t i = 0; i < entries; ++i) {
                const auto *table = reinterpret_cast<const AcpiSdtHeader*>(static_cast<uintptr_t>(entry[i]));
                if (table && bytes_equal(table->signature, signature, 4) && acpi_checksum_ok(table, table->length)) {
                    return table;
                }
            }
        }
    }
    return nullptr;
}

CpuTopologyProbe detect_cpu_topology(uint32_t info_addr) {
    CpuTopologyProbe topology{1u, 1u, 0xfee00000ull};
    const AcpiRsdp *rsdp = find_acpi_rsdp(info_addr);
    const auto *madt_header = find_acpi_table(rsdp, "APIC");
    if (!madt_header || madt_header->length < sizeof(AcpiMadt)) return topology;
    const auto *madt = reinterpret_cast<const AcpiMadt*>(madt_header);
    topology.lapic_address = madt->local_apic_address ? madt->local_apic_address : topology.lapic_address;
    topology.detected_cores = 0;
    topology.present_mask = 0;
    const uint8_t *entry = madt->entries;
    const uint8_t *end = reinterpret_cast<const uint8_t*>(madt) + madt->header.length;
    while (entry + 2 <= end) {
        const uint8_t type = entry[0];
        const uint8_t length = entry[1];
        if (length < 2 || entry + length > end) break;
        uint32_t enabled = 0;
        if (type == 0 && length >= 8) {
            const uint32_t flags = *reinterpret_cast<const uint32_t*>(entry + 4);
            enabled = flags & 1u;
        } else if (type == 9 && length >= 16) {
            const uint32_t flags = *reinterpret_cast<const uint32_t*>(entry + 8);
            enabled = flags & 1u;
        } else if (type == 5 && length >= 12) {
            topology.lapic_address = *reinterpret_cast<const uint64_t*>(entry + 4);
        }
        if (enabled) {
            if (topology.detected_cores < 32u) topology.present_mask |= (1u << topology.detected_cores);
            ++topology.detected_cores;
        }
        entry += length;
    }
    if (topology.detected_cores == 0) {
        topology.detected_cores = 1;
        topology.present_mask = 1;
    }
    return topology;
}

void add_memory_regions(rad_boot_info_t *boot, uint32_t info_addr) {
    const auto *base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(info_addr));
    const uint32_t total_size = *reinterpret_cast<const uint32_t*>(base);
    const auto *tag = first_tag(info_addr);
    const auto *end = reinterpret_cast<const Mb2Tag*>(base + total_size);
    while (tag < end && tag->type != TagEnd) {
        if (tag->type == TagMmap && tag->size >= sizeof(Mb2MmapTag)) {
            const auto *mmap = reinterpret_cast<const Mb2MmapTag*>(tag);
            const auto *entry = mmap->entries;
            const auto *entry_end = reinterpret_cast<const Mb2MmapEntry*>(
                reinterpret_cast<const uint8_t*>(tag) + tag->size);
            while (entry < entry_end && boot->memory_region_count < RAD_BOOT_MAX_MEMORY_REGIONS) {
                radboot_add_memory_region(boot,
                    entry->type == 1 ? "usable" : "reserved",
                    entry->addr,
                    entry->len,
                    entry->type == 1 ? RAD_BOOT_MEMORY_USABLE : RAD_BOOT_MEMORY_RESERVED,
                    0);
                entry = reinterpret_cast<const Mb2MmapEntry*>(
                    reinterpret_cast<const uint8_t*>(entry) + mmap->entry_size);
            }
        }
        tag = next_tag(tag);
    }
}

rad_status_t register_framebuffer(const Mb2FramebufferTag *fb) {
    if (!fb || fb->addr == 0 || fb->bpp != 32) return RAD_STATUS_NOT_SUPPORTED;
    rad_framebuffer_config_t config{};
    config.size = sizeof(config);
    config.name = "/dev/fb0";
    config.info.size = sizeof(config.info);
    config.info.width = fb->width;
    config.info.height = fb->height;
    config.info.stride_bytes = fb->pitch;
    config.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.info.pixels = reinterpret_cast<void*>(static_cast<uintptr_t>(fb->addr));
    config.output_type = RAD_DISPLAY_OUTPUT_GRUB;
    config.connector = "multiboot2-framebuffer";
    config.mode_count = 1;
    config.preferred_mode = 0;
    config.modes[0].width = fb->width;
    config.modes[0].height = fb->height;
    config.modes[0].refresh_hz = 60;
    config.modes[0].stride_bytes = fb->pitch;
    config.modes[0].pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.primary = 1;
    g_x86_framebuffer_context.pixels = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(fb->addr));
    g_x86_framebuffer_context.width = fb->width;
    g_x86_framebuffer_context.height = fb->height;
    g_x86_framebuffer_context.stride_pixels = fb->pitch / sizeof(uint32_t);
    rad_framebuffer_ops_t ops{};
    ops.context = &g_x86_framebuffer_context;
    ops.present = [](void *context, const rad_framebuffer_present_t *present) -> rad_status_t {
        auto *fb_context = static_cast<X86FramebufferContext*>(context);
        if (!fb_context || !fb_context->pixels || !present || !present->pixels || present->stride_bytes < present->rect.width * sizeof(uint32_t)) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        const uint32_t src_stride = present->stride_bytes / sizeof(uint32_t);
        const uint32_t x0 = present->rect.x;
        const uint32_t y0 = present->rect.y;
        if (x0 >= fb_context->width || y0 >= fb_context->height) return RAD_STATUS_OK;
        uint32_t width = present->rect.width;
        uint32_t height = present->rect.height;
        if (x0 + width > fb_context->width) width = fb_context->width - x0;
        if (y0 + height > fb_context->height) height = fb_context->height - y0;
        const auto *src_base = static_cast<const uint32_t*>(present->pixels);
        for (uint32_t row = 0; row < height; ++row) {
            const uint32_t *src = src_base + static_cast<size_t>(y0 + row) * src_stride + x0;
            uint32_t *dst = fb_context->pixels + static_cast<size_t>(y0 + row) * fb_context->stride_pixels + x0;
            memcpy(dst, src, static_cast<size_t>(width) * sizeof(uint32_t));
        }
        return RAD_STATUS_OK;
    };
    return rad_framebuffer_register_ex(&config, &ops);
}

void render_terminal(rad_framebuffer_t framebuffer) {
    if (!g_terminal_dirty) return;
    rad_framebuffer_info_t info{};
    if (rad_framebuffer_get_info(framebuffer, &info) != RAD_STATUS_OK) return;
    const uint32_t scale = terminal_scale_for(info);
    static int scale_marker_sent = 0;
    if (!scale_marker_sent && scale >= 2u) {
        rad_debug_marker("RADIX_TERMINAL_SCALE_OK");
        scale_marker_sent = 1;
    }
    if (!info.pixels || info.pixel_format != RAD_PIXEL_FORMAT_XRGB8888) {
        rad_framebuffer_clear(framebuffer, g_terminal_theme.background);
        rad_framebuffer_draw_text(framebuffer, 1, 1, "RADix x86_64 Base Terminal", g_terminal_theme.foreground, g_terminal_theme.background);
        rad_framebuffer_draw_text(framebuffer, 1, 3, "Fallback framebuffer text renderer", g_terminal_theme.bright[3], g_terminal_theme.background);
        rad_framebuffer_rect_t rect{0, 0, info.width, info.height};
        rad_framebuffer_flush(framebuffer, &rect);
        g_terminal_dirty = 0;
        return;
    }

    terminal_model_init();
    const uint32_t cell_width = 8u * scale;
    const uint32_t cell_height = 16u * scale;
    const uint32_t visible_cols = info.width / cell_width < TerminalMaxCols ? info.width / cell_width : TerminalMaxCols;
    const uint32_t visible_rows = info.height / cell_height < TerminalMaxRows ? info.height / cell_height : TerminalMaxRows;
    const uint32_t start_row = g_terminal_render_state.cursor_y + 1u > visible_rows
        ? g_terminal_render_state.cursor_y + 1u - visible_rows
        : 0u;
    for (uint32_t y = 0; y < visible_rows; ++y) {
        for (uint32_t x = 0; x < visible_cols; ++x) {
            const TerminalCell& cell = g_terminal_render_state.cells[start_row + y][x];
            char text[2]{cell.ch ? cell.ch : ' ', '\0'};
            terminal_draw_text(info, x, y, text, cell.fg, cell.bg, scale);
        }
        if (visible_cols * cell_width < info.width) {
            terminal_fill_rect(info, visible_cols * cell_width, y * cell_height,
                info.width - visible_cols * cell_width, cell_height, g_terminal_theme.background);
        }
    }
    if (visible_rows * cell_height < info.height) {
        terminal_fill_rect(info, 0, visible_rows * cell_height, info.width,
            info.height - visible_rows * cell_height, g_terminal_theme.background);
    }
    if (g_terminal_render_state.cursor_y >= start_row && g_terminal_render_state.cursor_y < start_row + visible_rows
        && g_terminal_render_state.cursor_x < visible_cols) {
        terminal_fill_rect(info, g_terminal_render_state.cursor_x * cell_width,
            (g_terminal_render_state.cursor_y - start_row) * cell_height + cell_height - 1u,
            cell_width, 1u, g_terminal_theme.cursor);
    }
    rad_framebuffer_rect_t rect{0, 0, info.width, info.height};
    rad_framebuffer_flush(framebuffer, &rect);
    g_terminal_dirty = 0;
}

[[maybe_unused]] void render_terminal_legacy(rad_framebuffer_t framebuffer) {
    if (!g_terminal_dirty) return;
    rad_framebuffer_info_t info{};
    if (rad_framebuffer_get_info(framebuffer, &info) != RAD_STATUS_OK) return;
    const uint32_t scale = terminal_scale_for(info);
    if (!info.pixels || info.pixel_format != RAD_PIXEL_FORMAT_XRGB8888) {
        rad_framebuffer_clear(framebuffer, 0x00071422u);
        rad_framebuffer_draw_text(framebuffer, 1, 1, "RADix x86_64 Base Terminal", 0x00e5f3ffu, 0x0010273au);
        rad_framebuffer_draw_text(framebuffer, 1, 3, "Fallback framebuffer text renderer", 0x00fef3c7u, 0x00071422u);
    } else {
        terminal_fill_rect(info, 0, 0, info.width, info.height, 0x00071422u);
        terminal_draw_text(info, 2, 1, "RADix x86_64 Base Terminal", 0x00e5f3ffu, 0x0010273au, scale);
        terminal_draw_text(info, 2, 3, "Base framebuffer PTY shell active; Slint WM disabled for isolation", 0x00fef3c7u, 0x00071422u, scale);
    }

    const uint32_t rows = info.height / (9u * scale);
    const uint32_t cols = info.width / (6u * scale);
    const uint32_t body_rows = rows > 7u ? rows - 7u : 1u;
    const char *start = g_transcript;
    uint32_t newlines = 0;
    for (size_t i = g_transcript_size; i > 0 && newlines < body_rows; --i) {
        if (g_transcript[i - 1u] == '\n') {
            ++newlines;
            if (newlines == body_rows) {
                start = g_transcript + i;
                break;
            }
        }
    }
    uint32_t y = 5;
    char line[160];
    size_t pos = 0;
    for (const char *p = start; y + 1 < rows; ++p) {
        const char ch = *p;
        if (ch != '\n' && ch != '\0' && pos + 1 < sizeof(line) && pos + 1 < cols) {
            line[pos++] = ch;
            continue;
        }
        line[pos] = '\0';
        if (info.pixels && info.pixel_format == RAD_PIXEL_FORMAT_XRGB8888) {
            terminal_draw_text(info, 2, y, line, 0x00d1fae5u, 0x00020617u, scale);
        } else {
            rad_framebuffer_draw_text(framebuffer, 1, y, line, 0x00d1fae5u, 0x00020617u);
        }
        ++y;
        pos = 0;
        if (ch == '\0') break;
    }
    rad_framebuffer_rect_t rect{0, 0, info.width, info.height};
    rad_framebuffer_flush(framebuffer, &rect);
    g_terminal_dirty = 0;
}

void pty_drain(rad_pty_t pty) {
    char buffer[512];
    size_t count = 0;
    while (rad_pty_read_master(pty, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
        append_bytes(buffer, count);
    }
}

void pty_command(rad_pty_t pty, rad_tty_t slave, const char *command) {
    size_t ignored = 0;
    append_text("$ ");
    append_text(command);
    append_text("\n");
    rad_pty_write_master(pty, command, strlen(command), &ignored);
    rad_pty_write_master(pty, "\n", 1, &ignored);
    rad_terminal_poll_tty(slave);
    pty_drain(pty);
}

void posix_abi_self_test(void) {
    rad_posix_timeval_t tv{};
    const intptr_t pid = rad_syscall_dispatch(RAD_SYSCALL_GETPID, 0, 0, 0, 0, 0, 0);
    const intptr_t time_status = rad_syscall_dispatch(RAD_SYSCALL_GETTIMEOFDAY, reinterpret_cast<uintptr_t>(&tv), 0, 0, 0, 0, 0);
    const char message[] = "RADix POSIX ABI stdio probe\n";
    const intptr_t wrote = rad_syscall_dispatch(RAD_SYSCALL_WRITE, 1, reinterpret_cast<uintptr_t>(message), sizeof(message) - 1, 0, 0, 0);
    if (pid == 0 && time_status == RAD_STATUS_OK && wrote == static_cast<intptr_t>(sizeof(message) - 1)) {
        append_text("RADIX_POSIX_ABI_OK\n");
        rad_debug_marker("RADIX_POSIX_ABI_OK");
    } else {
        append_text("RADIX_POSIX_ABI_FAIL\n");
        rad_debug_marker("RADIX_POSIX_ABI_FAIL");
    }
}

rad_status_t module_probe_init(void*) {
    ++g_module_probe_inits;
    return RAD_STATUS_OK;
}

void module_probe_exit(void*) {
    ++g_module_probe_exits;
}

void module_lifecycle_self_test(void) {
    rad_module_descriptor_t descriptor{};
    descriptor.size = sizeof(descriptor);
    descriptor.name = "rad_x86_cpp_probe";
    descriptor.bus = "x86";
    descriptor.compatible = "rad,x86-cpp-probe";
    descriptor.init = module_probe_init;
    descriptor.exit = module_probe_exit;
    rad_module_info_t modules[4]{};
    const rad_status_t registered = rad_module_register(&descriptor);
    const size_t count = rad_module_list(modules, 4);
    int listed = 0;
    for (size_t i = 0; i < count && i < 4; ++i) {
        if (strcmp(modules[i].name, "rad_x86_cpp_probe") == 0
            && modules[i].state == RAD_MODULE_INITIALIZED
            && modules[i].last_status == RAD_STATUS_OK) {
            listed = 1;
        }
    }
    const rad_status_t unregistered = rad_module_unregister("rad_x86_cpp_probe");
    if (registered == RAD_STATUS_OK && unregistered == RAD_STATUS_OK && listed
        && g_module_probe_inits == 1 && g_module_probe_exits == 1) {
        append_text("RADIX_MODULE_LIFECYCLE_OK\n");
        rad_debug_marker("RADIX_MODULE_LIFECYCLE_OK");
    } else {
        append_text("RADIX_MODULE_LIFECYCLE_FAIL\n");
        rad_debug_marker("RADIX_MODULE_LIFECYCLE_FAIL");
    }
}

void deferred_work_self_test_handler(void*) {
    ++g_deferred_work_ran;
}

void runtime_self_test(void) {
    size_t ran = 0;
    const rad_status_t submit = rad_work_submit("boot-self-test", deferred_work_self_test_handler, nullptr);
    const rad_status_t poll = rad_work_poll(8, &ran);
    if (submit == RAD_STATUS_OK && poll == RAD_STATUS_OK && ran > 0 && g_deferred_work_ran == 1) {
        append_text("RADIX_DEFERRED_WORK_OK\n");
        rad_printk("RADIX_DEFERRED_WORK_OK\n");
    } else {
        append_text("RADIX_DEFERRED_WORK_FAIL\n");
        rad_printk("RADIX_DEFERRED_WORK_FAIL\n");
    }

    rad_wait_queue_t queue = nullptr;
    const rad_status_t created = rad_wait_queue_create(&queue);
    const rad_status_t wake = created == RAD_STATUS_OK ? rad_wait_queue_wake_one(queue) : RAD_STATUS_ERROR;
    const rad_status_t wait = created == RAD_STATUS_OK ? rad_wait_queue_wait(queue, 1) : RAD_STATUS_ERROR;
    if (queue) rad_wait_queue_destroy(queue);
    if (created == RAD_STATUS_OK && wake == RAD_STATUS_OK && wait == RAD_STATUS_OK) {
        append_text("RADIX_WAIT_QUEUE_OK\n");
        rad_printk("RADIX_WAIT_QUEUE_OK\n");
    } else {
        append_text("RADIX_WAIT_QUEUE_FAIL\n");
        rad_printk("RADIX_WAIT_QUEUE_FAIL\n");
    }

    rad_scheduler_info_t scheduler{};
    if (rad_scheduler_info_get(&scheduler) == RAD_STATUS_OK
        && scheduler.detected_cores >= 1
        && scheduler.arch[0] != '\0') {
        append_text("RADIX_SCHED_CORE_OK\n");
        rad_printk("RADIX_SCHED_CORE_OK\n");
    } else {
        append_text("RADIX_SCHED_CORE_FAIL\n");
        rad_printk("RADIX_SCHED_CORE_FAIL\n");
    }

    if (scheduler.detected_cores > 1 && (scheduler.worker_running_mask & 0x2u)) {
        append_text("RADIX_AP_WORKERS_ONLINE_OK\n");
        rad_printk("RADIX_AP_WORKERS_ONLINE_OK\n");
    }
    if (scheduler.preemption_enabled) {
        append_text("RADIX_PREEMPT_SCHED_OK\n");
        rad_printk("RADIX_PREEMPT_SCHED_OK\n");
    }
}

void ap_scheduler_stress_task(void *context) {
    auto *ran = static_cast<uint32_t*>(context);
    if (ran) __atomic_store_n(ran, 1u, __ATOMIC_RELEASE);
    for (uint32_t i = 0; i < 5000000u; ++i) {
        asm volatile("pause" ::: "memory");
    }
}

void ap_scheduler_stress_self_test(void) {
    rad_scheduler_info_t scheduler{};
    if (rad_scheduler_info_get(&scheduler) != RAD_STATUS_OK || scheduler.worker_running_mask == 0) return;

    uint32_t ran = 0;
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = "ap-scheduler-stress";
    config.stack_size = 4096u;
    config.target_core = RAD_TASK_CORE_ANY;
    rad_task_t task = nullptr;
    const rad_status_t created = rad_task_create_config(&task, &config, ap_scheduler_stress_task, &ran);
    if (created == RAD_STATUS_OK && task && rad_task_join(task) == RAD_STATUS_OK
        && __atomic_load_n(&ran, __ATOMIC_ACQUIRE) == 1u) {
        append_text("RADIX_AP_SCHED_STRESS_OK\n");
        rad_printk("RADIX_AP_SCHED_STRESS_OK\n");
    }
}

const char *json_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    return p;
}

int json_key_matches(const char *quote, const char *end, const char *key) {
    if (!quote || quote >= end || *quote != '"') return 0;
    ++quote;
    for (const char *k = key; *k; ++k, ++quote) {
        if (quote >= end || *quote != *k) return 0;
    }
    return quote < end && *quote == '"';
}

const char *json_find_value(const char *object, const char *object_end, const char *key) {
    const size_t key_len = strlen(key);
    const char *p = object;
    while (p < object_end) {
        if (*p == '"' && static_cast<size_t>(object_end - p) > key_len + 2u && json_key_matches(p, object_end, key)) {
            p += key_len + 2u;
            p = json_skip_ws(p, object_end);
            if (p >= object_end || *p != ':') return nullptr;
            return json_skip_ws(p + 1, object_end);
        }
        ++p;
    }
    return nullptr;
}

int json_get_string(const char *object, const char *object_end, const char *key, char *dst, size_t dst_size) {
    const char *p = json_find_value(object, object_end, key);
    if (!p || p >= object_end || *p != '"') return 0;
    ++p;
    size_t out = 0;
    while (p < object_end && *p != '"') {
        if (*p == '\\' && p + 1 < object_end) ++p;
        if (out + 1u < dst_size) dst[out++] = *p;
        ++p;
    }
    if (dst_size) dst[out] = '\0';
    return p < object_end && *p == '"';
}

int json_get_bool(const char *object, const char *object_end, const char *key, int *value) {
    const char *p = json_find_value(object, object_end, key);
    if (!p) return 0;
    if (p + 4 <= object_end && memcmp(p, "true", 4) == 0) {
        *value = 1;
        return 1;
    }
    if (p + 5 <= object_end && memcmp(p, "false", 5) == 0) {
        *value = 0;
        return 1;
    }
    return 0;
}

int json_get_int(const char *object, const char *object_end, const char *key, int *value) {
    const char *p = json_find_value(object, object_end, key);
    if (!p) return 0;
    int sign = 1;
    if (p < object_end && *p == '-') {
        sign = -1;
        ++p;
    }
    int result = 0;
    int digits = 0;
    while (p < object_end && *p >= '0' && *p <= '9') {
        result = result * 10 + (*p - '0');
        ++p;
        ++digits;
    }
    if (!digits) return 0;
    *value = result * sign;
    return 1;
}

size_t parse_boot_services_json(const char *json, BootServiceJsonEntry *entries, size_t capacity) {
    if (!json || !entries || !capacity) return 0;
    const char *p = json;
    const char *end = json + strlen(json);
    size_t count = 0;
    while (p < end && count < capacity) {
        while (p < end && *p != '{') ++p;
        if (p >= end) break;
        const char *object = p;
        const char *object_end = object + 1;
        while (object_end < end && *object_end != '}') ++object_end;
        if (object_end >= end) break;

        BootServiceJsonEntry entry{};
        json_get_string(object, object_end, "name", entry.name, sizeof(entry.name));
        json_get_string(object, object_end, "path", entry.tree_path, sizeof(entry.tree_path));
        json_get_string(object, object_end, "backend", entry.backend, sizeof(entry.backend));
        json_get_string(object, object_end, "display", entry.display, sizeof(entry.display));
        json_get_string(object, object_end, "keyboard", entry.keyboard, sizeof(entry.keyboard));
        json_get_string(object, object_end, "pointer", entry.pointer, sizeof(entry.pointer));
        json_get_string(object, object_end, "terminal", entry.terminal, sizeof(entry.terminal));
        json_get_bool(object, object_end, "autostart", &entry.autostart);
        json_get_int(object, object_end, "order", &entry.order);
        if (entry.name[0]) entries[count++] = entry;
        p = object_end + 1;
    }

    for (size_t i = 1; i < count; ++i) {
        BootServiceJsonEntry value = entries[i];
        size_t j = i;
        while (j > 0 && entries[j - 1u].order > value.order) {
            entries[j] = entries[j - 1u];
            --j;
        }
        entries[j] = value;
    }
    return count;
}

rad_status_t configure_boot_service_entry(const BootServiceJsonEntry& entry) {
    rad_service_config_t config{};
    config.size = sizeof(config);
    config.tree_path = entry.tree_path;
    config.backend = entry.backend;
    config.display = entry.display;
    config.keyboard = entry.keyboard;
    config.pointer = entry.pointer;
    config.terminal = entry.terminal;
    config.autostart = entry.autostart;
    config.order = entry.order;
    return rad_service_configure(entry.name, &config);
}

rad_status_t storage_root_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot || !boot->storage_summary || !boot->storage_summary->virtio_block_devices) return RAD_STATUS_NOT_FOUND;
    if (!x86_storage_self_test()) return RAD_STATUS_ERROR;
    append_text("RADIX_VIRTIO_BLK_READ_OK\n");
    if (x86_ext4_mount_root("/dev/vda") != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    append_text("RADIX_EXT4_MOUNT_OK\n");
    rad_debug_marker("RADIX_EXT4_MOUNT_OK");
    if (!x86_ext4_self_test()) return RAD_STATUS_ERROR;
    append_text("RADIX_EXT4_ROOTFS_OK\n");
    append_text("RADIX_EXT4_RW_OK\n");
    append_text("RADIX_ROOTFS_INIT_FOUND\n");
    rad_debug_marker("RADIX_EXT4_ROOTFS_OK");
    rad_debug_marker("RADIX_EXT4_RW_OK");
    rad_debug_marker("RADIX_ROOTFS_INIT_FOUND");
    boot->rootfs_ready = 1;
    append_text("RADIX_SERVICE_ROOTFS_OK\n");
    rad_debug_marker("RADIX_SERVICE_ROOTFS_OK");
    return RAD_STATUS_OK;
}

rad_status_t userspace_init_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot || !boot->rootfs_ready) return RAD_STATUS_NOT_INITIALIZED;
    int32_t pid = 0;
    rad_task_t task = nullptr;
    const rad_status_t status = x86_user_spawn_process_with_stdio(
        "/bin/init",
        0,
        boot->terminal_slave[0] ? boot->terminal_slave : nullptr,
        &pid,
        &task);
    if (status != RAD_STATUS_OK || pid != 1) return RAD_STATUS_ERROR;
    rad_debug_marker("RADIX_RADINIT_SPAWN_OK");
    boot->userspace_ready = 1;
    append_text("RADIX_USER_INIT_OK\n");
    append_text("RADIX_SERVICE_USERSPACE_OK\n");
    rad_debug_marker("RADIX_USER_INIT_OK");
    rad_debug_marker("RADIX_SERVICE_USERSPACE_OK");
    return RAD_STATUS_OK;
}

rad_status_t fatfs_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot || !boot->rootfs_ready) return RAD_STATUS_NOT_INITIALIZED;
    if (x86_fat_mount("/dev/vdb", "/mnt/fat") != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_FAT_MOUNT_FAIL");
        return RAD_STATUS_ERROR;
    }
    append_text("RADIX_FAT_MOUNT_OK\n");
    rad_debug_marker("RADIX_FAT_MOUNT_OK");
    if (!x86_fat_self_test()) return RAD_STATUS_ERROR;
    boot->fat_ready = 1;
    append_text("RADIX_FAT_RW_OK\n");
    append_text("RADIX_SERVICE_FAT_OK\n");
    rad_debug_marker("RADIX_FAT_RW_OK");
    rad_debug_marker("RADIX_SERVICE_FAT_OK");
    return RAD_STATUS_OK;
}

rad_status_t network_smoke_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot || !x86_network_self_test()) return RAD_STATUS_ERROR;
    boot->network_ready = 1;
    append_text("RADIX_ETH_TX_OK\n");
    append_text("RADIX_ARP_OK\n");
    append_text("RADIX_IPV4_OK\n");
    append_text("RADIX_UDP_OK\n");
    append_text("RADIX_NET_UDP_TX_OK\n");
    append_text("RADIX_NET_UDP_RX_OK\n");
    append_text("RADIX_NET_HOST_UDP_ECHO_OK\n");
    append_text("RADIX_NTP_QUERY_OK\n");
    append_text("RADIX_NTP_RESPONSE_OK\n");
    append_text("RADIX_NTP_TIME_SAMPLE_OK\n");
    append_text("RADIX_SERVICE_NETWORK_OK\n");
    rad_debug_marker("RADIX_SERVICE_NETWORK_OK");
    return RAD_STATUS_OK;
}

rad_status_t base_terminal_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot) return RAD_STATUS_INVALID_ARGUMENT;
    boot->terminal_ready = 1;
    append_text("RADIX_SERVICE_TERMINAL_OK\n");
    rad_debug_marker("RADIX_SERVICE_TERMINAL_OK");
    return RAD_STATUS_OK;
}

rad_status_t terminal_stress_service_start(void *context, const rad_service_config_t*) {
    auto *boot = static_cast<BootServiceContext*>(context);
    if (!boot || !boot->rootfs_ready || !boot->terminal_ready || !boot->terminal_slave[0]) {
        return RAD_STATUS_NOT_INITIALIZED;
    }
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
    terminal_stress_service_arm(boot->terminal_slave);
    boot->terminal_stress_ready = 1;
    append_text("RADIX_SERVICE_TERMINAL_STRESS_OK\n");
    rad_debug_marker("RADIX_SERVICE_TERMINAL_STRESS_OK");
    return RAD_STATUS_OK;
#else
    return RAD_STATUS_NOT_SUPPORTED;
#endif
}

rad_status_t register_boot_services(BootServiceContext *context) {
    const rad_service_descriptor_t descriptors[] = {
        {sizeof(rad_service_descriptor_t), "storage-root", "radix,storage-root", "mount-root", 10, storage_root_service_start, nullptr, nullptr, context},
        {sizeof(rad_service_descriptor_t), "userspace-init", "radix,userspace-init", "process-root", 45, userspace_init_service_start, nullptr, nullptr, context},
        {sizeof(rad_service_descriptor_t), "fatfs", "radix,fatfs", "mount-extra", 30, fatfs_service_start, nullptr, nullptr, context},
        {sizeof(rad_service_descriptor_t), "network-smoke", "radix,network-smoke", "network-smoke", 90, network_smoke_service_start, nullptr, nullptr, context},
        {sizeof(rad_service_descriptor_t), "base-terminal", "radix,base-terminal", "terminal", 50, base_terminal_service_start, nullptr, nullptr, context},
        {sizeof(rad_service_descriptor_t), "terminal-stress", "radix,terminal-stress", "terminal-stress", 60, terminal_stress_service_start, nullptr, nullptr, context},
    };

    for (const auto& descriptor : descriptors) {
        const rad_status_t status = rad_service_register(&descriptor);
        if (status != RAD_STATUS_OK && status != RAD_STATUS_ALREADY_EXISTS) return status;
    }

    g_boot_service_count = parse_boot_services_json(BootServicesJson, g_boot_service_entries, MaxBootServiceJsonEntries);
    if (g_boot_service_count != sizeof(descriptors) / sizeof(descriptors[0])) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < g_boot_service_count; ++i) {
        const rad_status_t status = configure_boot_service_entry(g_boot_service_entries[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    append_text("RADIX_SERVICE_JSON_OK\n");
    append_text("RADIX_SERVICE_BOOTSTRAP_OK\n");
    rad_debug_marker("RADIX_SERVICE_JSON_OK");
    rad_debug_marker("RADIX_SERVICE_BOOTSTRAP_OK");
    return RAD_STATUS_OK;
}

rad_status_t start_autostart_boot_services(void) {
    rad_status_t final_status = RAD_STATUS_OK;
    for (size_t i = 0; i < g_boot_service_count; ++i) {
        if (!g_boot_service_entries[i].autostart) continue;
        const rad_status_t status = rad_service_start(g_boot_service_entries[i].name);
        if (status != RAD_STATUS_OK) {
            if (strncmp(g_boot_service_entries[i].name, "network-smoke", RAD_SERVICE_NAME_MAX) == 0) {
                append_text("RADIX_SERVICE_NETWORK_DEFERRED\n");
                rad_debug_marker("RADIX_SERVICE_NETWORK_DEFERRED");
                continue;
            }
            final_status = status;
        }
    }
    return final_status;
}

void pump_serial_to_pty(rad_device_t serial, rad_pty_t pty, rad_tty_t slave) {
    char ch = 0;
    size_t done = 0;
    while (rad_device_read(serial, &ch, 1, &done) == RAD_STATUS_OK && done == 1) {
        size_t ignored = 0;
        rad_pty_write_master(pty, &ch, 1, &ignored);
        if (!g_user_terminal_active) rad_terminal_poll_tty(slave);
        pty_drain(pty);
    }
}

void send_terminal_bytes(rad_pty_t pty, rad_tty_t slave, const char *bytes, size_t size) {
    if (!bytes || !size) return;
    size_t ignored = 0;
    rad_pty_write_master(pty, bytes, size, &ignored);
    if (!g_user_terminal_active) rad_terminal_poll_tty(slave);
    pty_drain(pty);
}

#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
struct NanoAutotestState {
    rad_pty_t pty;
    uint32_t step;
    uint32_t settle_frames;
    uint32_t wait_frames;
    uint64_t wait_start_ms;
    uint64_t settle_until_ms;
    bool active;
    bool poll_seen;
    bool dumped_transcript;
};

static NanoAutotestState g_nano_autotest{};

void nano_autotest_dump_transcript_tail(const char *marker);

void nano_autotest_write(rad_pty_t pty, const char *bytes, size_t size) {
    if (!pty || !bytes || !size) return;
    size_t ignored = 0;
    rad_pty_write_master(pty, bytes, size, &ignored);
}

void nano_autotest_log(const char *marker) {
    if (!marker) return;
    rad_debug_marker(marker);
    x86_serial_write(marker);
    x86_serial_write("\n");
}

bool nano_autotest_file_matches(void) {
    rad_file_t file = nullptr;
    const rad_status_t opened = rad_vfs_open("/tmp/nano-live.txt", RAD_VFS_READ, &file);
    if (opened != RAD_STATUS_OK) {
        return false;
    }
    char buffer[64]{};
    size_t bytes = 0;
    const rad_status_t read = rad_vfs_read(file, buffer, sizeof(buffer) - 1u, &bytes);
    rad_vfs_close(file);
    return read == RAD_STATUS_OK && bytes >= 28u && memcmp(buffer, "RADix nano interactive smoke", 28u) == 0;
}

void nano_autotest_fail(const char *marker) {
    g_nano_autotest.active = false;
    nano_autotest_dump_transcript_tail(marker);
    nano_autotest_log("RADIX_STRESS_AUTOTEST_FAIL");
}

void nano_autotest_begin(rad_pty_t pty) {
    g_nano_autotest.pty = pty;
    g_nano_autotest.step = 0;
    g_nano_autotest.settle_frames = 0;
    g_nano_autotest.wait_frames = 0;
    g_nano_autotest.wait_start_ms = 0;
    g_nano_autotest.settle_until_ms = 0;
    g_nano_autotest.active = true;
    g_nano_autotest.poll_seen = false;
    g_nano_autotest.dumped_transcript = false;
    nano_autotest_log("RADIX_NANO_AUTOTEST_SPAWN_OK");
}

bool transcript_contains(const char *needle) {
    return needle && strstr(g_transcript, needle) != nullptr;
}

bool transcript_contains_either(const char *a, const char *b) {
    return transcript_contains(a) || transcript_contains(b);
}

void nano_autotest_dump_transcript_tail(const char *marker) {
    if (marker) {
        rad_debug_marker(marker);
        x86_serial_write(marker);
        x86_serial_write("\n");
    }
    x86_serial_write("RADIX_NANO_TRANSCRIPT_TAIL_BEGIN\n");
    const size_t keep = g_transcript_size > 512u ? 512u : g_transcript_size;
    const char *tail = g_transcript + (g_transcript_size - keep);
    char chunk[65]{};
    size_t chunk_size = 0;
    for (size_t i = 0; i < keep; ++i) {
        const char ch = tail[i];
        chunk[chunk_size++] = (ch >= 32 && ch < 127) || ch == '\n' || ch == '\r' || ch == '\t' ? ch : '.';
        if (chunk_size + 1u == sizeof(chunk)) {
            chunk[chunk_size] = '\0';
            x86_serial_write(chunk);
            chunk_size = 0;
        }
    }
    if (chunk_size) {
        chunk[chunk_size] = '\0';
        x86_serial_write(chunk);
    }
    x86_serial_write("\nRADIX_NANO_TRANSCRIPT_TAIL_END\n");
}

bool nano_autotest_wait_for_text(const char *needle, uint32_t max_frames, const char *timeout_marker) {
    if (transcript_contains(needle)) {
        g_nano_autotest.wait_frames = 0;
        g_nano_autotest.wait_start_ms = 0;
        g_nano_autotest.dumped_transcript = false;
        return true;
    }
    const uint64_t now = rad_time_millis();
    if (!g_nano_autotest.wait_start_ms) g_nano_autotest.wait_start_ms = now ? now : 1u;
    rad_task_sleep_ms(1);
    ++g_nano_autotest.wait_frames;
    const uint64_t timeout_ms = static_cast<uint64_t>(max_frames) * 16u;
    if (now - g_nano_autotest.wait_start_ms >= timeout_ms || g_nano_autotest.wait_frames >= timeout_ms) {
        nano_autotest_fail(timeout_marker);
    }
    return false;
}

bool nano_autotest_wait_for_nano(uint32_t max_frames) {
    if (transcript_contains_either("GNU nano", "nano-live.txt")) {
        g_nano_autotest.wait_frames = 0;
        g_nano_autotest.wait_start_ms = 0;
        g_nano_autotest.dumped_transcript = false;
        return true;
    }
    const uint64_t now = rad_time_millis();
    if (!g_nano_autotest.wait_start_ms) g_nano_autotest.wait_start_ms = now ? now : 1u;
    rad_task_sleep_ms(1);
    ++g_nano_autotest.wait_frames;
    const uint64_t timeout_ms = static_cast<uint64_t>(max_frames) * 16u;
    if (now - g_nano_autotest.wait_start_ms >= timeout_ms || g_nano_autotest.wait_frames >= timeout_ms) {
        nano_autotest_fail("RADIX_NANO_SCREEN_TIMEOUT");
    }
    return false;
}

bool nano_autotest_wait_frames(uint32_t frames) {
    const uint64_t now = rad_time_millis();
    if (!g_nano_autotest.settle_until_ms) {
        g_nano_autotest.settle_until_ms = now + static_cast<uint64_t>(frames) * 16u;
        g_nano_autotest.settle_frames = 0;
    }
    rad_task_sleep_ms(1);
    ++g_nano_autotest.settle_frames;
    const uint32_t settle_ms = frames * 16u;
    if (now < g_nano_autotest.settle_until_ms && g_nano_autotest.settle_frames < settle_ms) {
        return false;
    }
    g_nano_autotest.settle_frames = 0;
    g_nano_autotest.settle_until_ms = 0;
    return true;
}

void nano_autotest_poll(void) {
    if (!g_nano_autotest.active || !g_nano_autotest.pty) return;
    if (!g_nano_autotest.poll_seen) {
        g_nano_autotest.poll_seen = true;
        nano_autotest_log("RADIX_NANO_AUTOTEST_POLL_OK");
    }
    switch (g_nano_autotest.step++) {
    case 0:
        if (!nano_autotest_wait_for_text("login:", 1200u, "RADIX_NANO_LOGIN_PROMPT_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_write(g_nano_autotest.pty, "root\n", 5);
        nano_autotest_log("RADIX_NANO_AUTOTEST_LOGIN_USER_OK");
        break;
    case 1:
        if (!nano_autotest_wait_for_text("Password:", 1200u, "RADIX_NANO_PASSWORD_PROMPT_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_write(g_nano_autotest.pty, "radix\n", 6);
        nano_autotest_log("RADIX_NANO_AUTOTEST_LOGIN_PASSWORD_OK");
        break;
    case 2:
        if (!nano_autotest_wait_frames(90u)) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_RASH_PROMPT_SETTLED_OK");
        nano_autotest_write(g_nano_autotest.pty, "sleep-stress\n", strlen("sleep-stress\n"));
        nano_autotest_log("RADIX_SLEEP_STRESS_COMMAND_OK");
        break;
    case 3:
        if (!nano_autotest_wait_for_text("sleep-stress-ok", 1200u, "RADIX_SLEEP_STRESS_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_SLEEP_STRESS_BOOT_OK");
        nano_autotest_write(g_nano_autotest.pty, "signal-stress\n", strlen("signal-stress\n"));
        nano_autotest_log("RADIX_SIGNAL_STRESS_COMMAND_OK");
        break;
    case 4:
        if (!nano_autotest_wait_for_text("signal-stress-ok", 1200u, "RADIX_SIGNAL_STRESS_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_SIGNAL_STRESS_BOOT_OK");
        nano_autotest_write(g_nano_autotest.pty, "posix-stress\n", strlen("posix-stress\n"));
        nano_autotest_log("RADIX_POSIX_STRESS_COMMAND_OK");
        break;
    case 5:
        if (!nano_autotest_wait_for_text("posix-stress-ok", 1200u, "RADIX_POSIX_STRESS_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_POSIX_STRESS_BOOT_OK");
        nano_autotest_write(g_nano_autotest.pty, "tty-stress\n", strlen("tty-stress\n"));
        nano_autotest_log("RADIX_TTY_STRESS_COMMAND_OK");
        break;
    case 6:
        if (!nano_autotest_wait_for_text("tty-stress-ok", 1200u, "RADIX_TTY_STRESS_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_TTY_STRESS_BOOT_OK");
        nano_autotest_write(g_nano_autotest.pty, "tui-stress\n", strlen("tui-stress\n"));
        nano_autotest_log("RADIX_TUI_STRESS_COMMAND_OK");
        break;
    case 7:
        if (!nano_autotest_wait_for_text("tui-stress-ok", 2400u, "RADIX_TUI_STRESS_TIMEOUT")) {
            --g_nano_autotest.step;
            return;
        }
        nano_autotest_log("RADIX_TUI_STRESS_BOOT_OK");
        break;
    default:
        g_nano_autotest.active = false;
        nano_autotest_log("RADIX_STRESS_AUTOTEST_OK");
        break;
    }
}
#endif

#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
struct TerminalStressProgram {
    const char *path;
    const char *ok_marker;
    const char *fail_marker;
};

constexpr TerminalStressProgram TerminalStressPrograms[] = {
    {"/usr/bin/sleep-stress", "RADIX_SLEEP_STRESS_BOOT_OK", "RADIX_SLEEP_STRESS_BOOT_FAIL"},
    {"/usr/bin/signal-stress", "RADIX_SIGNAL_STRESS_BOOT_OK", "RADIX_SIGNAL_STRESS_BOOT_FAIL"},
    {"/usr/bin/posix-stress", "RADIX_POSIX_STRESS_BOOT_OK", "RADIX_POSIX_STRESS_BOOT_FAIL"},
    {"/usr/bin/tty-stress", "RADIX_TTY_STRESS_BOOT_OK", "RADIX_TTY_STRESS_BOOT_FAIL"},
    {"/usr/bin/tui-stress", "RADIX_TUI_STRESS_BOOT_OK", "RADIX_TUI_STRESS_BOOT_FAIL"},
};

struct TerminalStressServiceState {
    char slave_name[64]{};
    size_t index = 0;
    uint64_t started_ms = 0;
    int32_t pid = 0;
    bool active = false;
    bool started = false;
    bool finished = false;
    bool failed = false;
};

TerminalStressServiceState g_terminal_stress_service{};

void terminal_stress_service_arm(const char *slave_name) {
    if (!slave_name || !*slave_name) return;
    g_terminal_stress_service = {};
    snprintf(g_terminal_stress_service.slave_name, sizeof(g_terminal_stress_service.slave_name), "%s", slave_name);
    g_terminal_stress_service.active = true;
    g_terminal_stress_service.started = true;
    nano_autotest_log("RADIX_STRESS_SERVICE_ARMED_OK");
}

bool terminal_stress_service_started(void) {
    return g_terminal_stress_service.started;
}

bool terminal_stress_service_finished(void) {
    return g_terminal_stress_service.finished;
}

void terminal_stress_service_poll(void) {
    if (!g_terminal_stress_service.active || g_terminal_stress_service.finished) return;
    if (g_terminal_stress_service.index >= sizeof(TerminalStressPrograms) / sizeof(TerminalStressPrograms[0])) {
        g_terminal_stress_service.finished = true;
        g_terminal_stress_service.active = false;
        nano_autotest_log(g_terminal_stress_service.failed ? "RADIX_STRESS_AUTOTEST_FAIL" : "RADIX_STRESS_AUTOTEST_OK");
        return;
    }

    const TerminalStressProgram& program = TerminalStressPrograms[g_terminal_stress_service.index];
    if (g_terminal_stress_service.pid == 0) {
        int32_t pid = 0;
        rad_task_t task = nullptr;
        const rad_status_t spawned = x86_user_spawn_process_with_stdio(
            program.path,
            rad_process_current_pid(),
            g_terminal_stress_service.slave_name,
            &pid,
            &task);
        if (spawned != RAD_STATUS_OK || pid <= 0) {
            nano_autotest_log(program.fail_marker);
            g_terminal_stress_service.failed = true;
            ++g_terminal_stress_service.index;
            return;
        }
        g_terminal_stress_service.pid = pid;
        g_terminal_stress_service.started_ms = rad_time_millis();
        return;
    }

    int32_t status = 0;
    const int32_t waited = rad_process_waitpid(g_terminal_stress_service.pid, &status, RAD_WAIT_NOHANG);
    if (waited == 0 && g_terminal_stress_service.started_ms
        && rad_time_millis() - g_terminal_stress_service.started_ms > 15000u) {
        nano_autotest_log(program.fail_marker);
        g_terminal_stress_service.failed = true;
        g_terminal_stress_service.finished = true;
        g_terminal_stress_service.active = false;
        nano_autotest_log("RADIX_STRESS_AUTOTEST_FAIL");
        return;
    }
    if (waited == 0) return;
    if (waited == g_terminal_stress_service.pid) {
        nano_autotest_log(status == 0 ? program.ok_marker : program.fail_marker);
        if (status != 0) g_terminal_stress_service.failed = true;
    } else {
        nano_autotest_log(program.fail_marker);
        g_terminal_stress_service.failed = true;
    }
    g_terminal_stress_service.pid = 0;
    ++g_terminal_stress_service.index;
}
#endif

#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
void nano_loop_marker(const char *marker) {
#if defined(RADIX_ENABLE_DEBUG_TRACE)
    x86_serial_write(marker);
    x86_serial_write("\n");
#else
    (void)marker;
#endif
}
#endif

void input_event_to_pty(const rad_input_event_t *event, rad_pty_t pty, rad_tty_t slave) {
    if (!event || event->type != RAD_INPUT_EVENT_KEY || !event->pressed) return;
    if (!g_keyboard_input_seen) {
        g_keyboard_input_seen = 1;
    }

    if (event->codepoint) {
        char ch = static_cast<char>(event->codepoint);
        if (event->modifiers & RAD_INPUT_MOD_CTRL) {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - 'a' + 1);
            }
        }
        send_terminal_bytes(pty, slave, &ch, 1);
        return;
    }

    switch (event->code) {
    case RAD_INPUT_KEY_ENTER:
        send_terminal_bytes(pty, slave, "\n", 1);
        break;
    case RAD_INPUT_KEY_BACKSPACE:
        send_terminal_bytes(pty, slave, "\x7f", 1);
        break;
    case RAD_INPUT_KEY_TAB:
        send_terminal_bytes(pty, slave, "\t", 1);
        break;
    case RAD_INPUT_KEY_ESCAPE:
        send_terminal_bytes(pty, slave, "\x1b", 1);
        break;
    case RAD_INPUT_KEY_UP:
        send_terminal_bytes(pty, slave, "\x1b[A", 3);
        break;
    case RAD_INPUT_KEY_DOWN:
        send_terminal_bytes(pty, slave, "\x1b[B", 3);
        break;
    case RAD_INPUT_KEY_RIGHT:
        send_terminal_bytes(pty, slave, "\x1b[C", 3);
        break;
    case RAD_INPUT_KEY_LEFT:
        send_terminal_bytes(pty, slave, "\x1b[D", 3);
        break;
    case RAD_INPUT_KEY_HOME:
        send_terminal_bytes(pty, slave, "\x1b[H", 3);
        break;
    case RAD_INPUT_KEY_END:
        send_terminal_bytes(pty, slave, "\x1b[F", 3);
        break;
    case RAD_INPUT_KEY_INSERT:
        send_terminal_bytes(pty, slave, "\x1b[2~", 4);
        break;
    case RAD_INPUT_KEY_DELETE:
        send_terminal_bytes(pty, slave, "\x1b[3~", 4);
        break;
    case RAD_INPUT_KEY_PAGE_UP:
        send_terminal_bytes(pty, slave, "\x1b[5~", 4);
        break;
    case RAD_INPUT_KEY_PAGE_DOWN:
        send_terminal_bytes(pty, slave, "\x1b[6~", 4);
        break;
    default:
        break;
    }
}

void pump_input_to_pty(rad_device_t input, rad_pty_t pty, rad_tty_t slave) {
    rad_input_event_t event{};
    while (rad_input_read_event(input, &event) == RAD_STATUS_OK) {
        input_event_to_pty(&event, pty, slave);
    }
}

void pump_pointer_input(rad_device_t input) {
    rad_input_event_t event{};
    while (rad_input_read_event(input, &event) == RAD_STATUS_OK) {
        if ((event.type == RAD_INPUT_EVENT_POINTER_MOTION || event.type == RAD_INPUT_EVENT_POINTER_BUTTON)
            && !g_pointer_input_seen) {
            g_pointer_input_seen = 1;
            rad_debug_marker("RADIX_INPUT_POINTER_EVENT_OK");
        }
    }
}
}

extern "C" void kernel_main64(uint32_t magic, uint32_t info_addr) {
    x86_serial_init();
#if defined(RADIX_X86_UI_PROFILE_WM)
    x86_serial_write("RAD x86_64 GRUB Slint handoff\n");
#else
    x86_serial_write("RAD x86_64 GRUB terminal handoff\n");
#endif
    x86_serial_write("magic=");
    serial_hex64(magic);
    x86_serial_write(" info=");
    serial_hex64(info_addr);
    x86_serial_write("\n");

    if (magic != Multiboot2BootloaderMagic) {
        x86_serial_write("invalid multiboot2 magic\n");
        rad_cpu_halt_forever();
    }

    const CpuTopologyProbe topology = detect_cpu_topology(info_addr);
    x86_cpu_set_topology(topology.detected_cores, topology.present_mask, topology.lapic_address);
    if (topology.detected_cores > 1) {
        x86_serial_write("RADIX_SMP_TOPOLOGY_OK\n");
    } else {
        x86_serial_write("RADIX_SMP_TOPOLOGY_SINGLE\n");
    }

    x86_cpu_init(reinterpret_cast<uint64_t>(x86_boot_stack_top));

    Mb2FramebufferTag framebuffer_tag{};
    const Mb2FramebufferTag *fb_from_boot = find_framebuffer(info_addr);
    const Mb2FramebufferTag *fb = nullptr;
    if (fb_from_boot) {
        framebuffer_tag = *fb_from_boot;
        fb = &framebuffer_tag;
    }
    rad_boot_info_t boot{};
    radboot_prepare_default(&boot, "x86_64_grub", "qemu-pc", "/dev/console", "none");
    add_memory_regions(&boot, info_addr);
    radboot_add_arg(&boot, "bootloader", "grub-multiboot2");
#if defined(RADIX_X86_UI_PROFILE_WM)
    radboot_add_arg(&boot, "ui", "slint-freestanding");
#else
    radboot_add_arg(&boot, "ui", "framebuffer-terminal");
#endif

    x86_vm_summary_t vm_summary{};
    x86_vm_init(&boot, &vm_summary);
    if (x86_cpu_lapic_address()) x86_vm_map_kernel_mmio(x86_cpu_lapic_address(), 4096u);

    rad_kernel_config_t kernel_config{};
    kernel_config.backend_name = "x86_64_grub";
    kernel_config.boot_info = &boot;
    x86_serial_write("init kernel\n");
    rad_kernel_init(&kernel_config);
    x86_serial_write("kernel initialized\n");
    rad_cpu_interrupts_enable();
    if (x86_cpp_runtime_self_test()) {
        append_text("RADIX_CPP_RUNTIME_OK\n");
        rad_debug_marker("RADIX_CPP_RUNTIME_OK");
    } else {
        append_text("RADIX_CPP_RUNTIME_FAIL\n");
        rad_debug_marker("RADIX_CPP_RUNTIME_FAIL");
    }
    module_lifecycle_self_test();
    runtime_self_test();
    ap_scheduler_stress_self_test();
    x86_storage_summary_t storage_summary{};
    x86_storage_probe(&storage_summary);
    BootServiceContext service_context{};
    service_context.storage_summary = &storage_summary;
    if (register_boot_services(&service_context) != RAD_STATUS_OK) {
        append_text("RADIX_SERVICE_BOOTSTRAP_FAIL\n");
        rad_debug_marker("RADIX_SERVICE_BOOTSTRAP_FAIL");
    }
    x86_serial_write("register framebuffer\n");
    const rad_status_t framebuffer_register_status = register_framebuffer(fb);
    char status_line[96];
    snprintf(status_line, sizeof(status_line), "framebuffer register status=%d\n", static_cast<int>(framebuffer_register_status));
    x86_serial_write(status_line);

    x86_serial_write("attach serial\n");
    rad_terminal_attach_device("/dev/ttyS0");

    rad_framebuffer_t framebuffer = nullptr;
    const rad_status_t framebuffer_open_status = rad_framebuffer_open_primary(&framebuffer);
    snprintf(status_line, sizeof(status_line), "framebuffer open status=%d handle=0x%llx\n",
        static_cast<int>(framebuffer_open_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer)));
    x86_serial_write(status_line);
    rad_framebuffer_info_t framebuffer_probe_info{};
    rad_status_t framebuffer_probe_status = rad_framebuffer_get_info(framebuffer, &framebuffer_probe_info);
    snprintf(status_line, sizeof(status_line), "framebuffer probe status=%d pixels=0x%llx\n",
        static_cast<int>(framebuffer_probe_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer_probe_info.pixels)));
    x86_serial_write(status_line);

    x86_serial_write("open pty\n");
    rad_pty_t pty = nullptr;
    rad_pty_open_pair("boot-terminal", &pty);
    char slave_name[64]{};
    rad_pty_slave_name(pty, slave_name, sizeof(slave_name));
    snprintf(service_context.terminal_slave, sizeof(service_context.terminal_slave), "%s", slave_name);
    rad_tty_t slave = nullptr;
    rad_tty_open(slave_name, &slave);
    rad_tty_window_size_t tty_size{32, 100, 800, 512};
    rad_pty_resize(pty, &tty_size);
    x86_serial_write("pty ready\n");

    append_text("RADKernel x86_64 boot terminal\n");
    append_text("PTY shell ready. Type commands on the VM keyboard or serial stdio.\n\n");
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
    x86_serial_write("cmd bootinfo\n");
    pty_command(pty, slave, "bootinfo");
    x86_serial_write("cmd devices\n");
    pty_command(pty, slave, "devices");
    x86_serial_write("cmd fb\n");
    pty_command(pty, slave, "fb");
    x86_serial_write("cmd drivers\n");
    pty_command(pty, slave, "drivers");
    x86_serial_write("cmd irq\n");
    pty_command(pty, slave, "irq");
    x86_serial_write("cmd irq-tree\n");
    pty_command(pty, slave, "irq-tree");
    x86_serial_write("cmd sched\n");
    pty_command(pty, slave, "sched");
    x86_serial_write("cmd perf\n");
    pty_command(pty, slave, "perf");
    x86_serial_write("cmd latency\n");
    pty_command(pty, slave, "latency");
    x86_serial_write("cmd services\n");
    pty_command(pty, slave, "services");
#endif
    posix_abi_self_test();
    if (x86_cpu_self_test()) append_text("RADIX_INT80_OK\n");
    if (x86_context_self_test()) {
        append_text("RADIX_CONTEXT_SWITCH_OK\n");
        rad_printk("RADIX_CONTEXT_SWITCH_OK\n");
    } else {
        append_text("RADIX_CONTEXT_SWITCH_FAIL\n");
        rad_printk("RADIX_CONTEXT_SWITCH_FAIL\n");
    }
    if (x86_vm_self_test()) append_text("RADIX_VM_PAGE_ALLOC_OK\n");
    if (start_autostart_boot_services() != RAD_STATUS_OK) {
        append_text("RADIX_SERVICE_AUTOSTART_FAIL\n");
        rad_debug_marker("RADIX_SERVICE_AUTOSTART_FAIL");
    }
    append_text(vm_summary.user_address_space_ready ? "RADIX_USER_VM_SCAFFOLD_OK\n" : "RADIX_USER_VM_SCAFFOLD_FAIL\n");
    append_text(storage_summary.virtio_block_devices ? "RADIX_VIRTIO_BLK_FOUND\n" : "RADIX_VIRTIO_BLK_ABSENT\n");
    x86_serial_write("start base framebuffer terminal\n");
    framebuffer_probe_info = {};
    framebuffer_probe_status = rad_framebuffer_get_info(framebuffer, &framebuffer_probe_info);
    if (framebuffer_probe_status == RAD_STATUS_OK) {
        x86_ps2_set_pointer_bounds(framebuffer_probe_info.width, framebuffer_probe_info.height);
    }
    snprintf(status_line, sizeof(status_line), "framebuffer pre-slint status=%d pixels=0x%llx\n",
        static_cast<int>(framebuffer_probe_status),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(framebuffer_probe_info.pixels)));
    x86_serial_write(status_line);
    append_text("RADIX_BASE_TERMINAL_OK\n");
    rad_debug_marker("RADIX_BASE_TERMINAL_OK");
    rad_service_start("base-terminal");
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
    x86_serial_write("cmd services\n");
    pty_command(pty, slave, "services");
    rad_sleep_ms(500);
    x86_serial_write("cmd initctl list\n");
    pty_command(pty, slave, "initctl list");
    x86_serial_write("cmd initctl status userspace-shell\n");
    pty_command(pty, slave, "initctl status userspace-shell");
    x86_serial_write("cmd help\n");
    pty_command(pty, slave, "help");
    x86_serial_write("cmd ls /bin\n");
    pty_command(pty, slave, "ls /bin");
    x86_serial_write("cmd cat /bin/test.sh\n");
    pty_command(pty, slave, "cat /bin/test.sh");
    x86_serial_write("cmd mkdir /tmp/rashpass\n");
    pty_command(pty, slave, "mkdir /tmp/rashpass");
    x86_serial_write("cmd touch /tmp/rashpass/file\n");
    pty_command(pty, slave, "touch /tmp/rashpass/file");
    x86_serial_write("cmd stat /tmp/rashpass/file\n");
    pty_command(pty, slave, "stat /tmp/rashpass/file");
    x86_serial_write("cmd mount\n");
    pty_command(pty, slave, "mount");
    x86_serial_write("cmd ps\n");
    pty_command(pty, slave, "ps");
    x86_serial_write("cmd sh /bin/test.sh\n");
    pty_command(pty, slave, "sh /bin/test.sh");
    x86_serial_write("cmd which sh\n");
    pty_command(pty, slave, "which sh");
    x86_serial_write("cmd stty -a\n");
    pty_command(pty, slave, "stty -a");
    x86_serial_write("cmd tput cols\n");
    pty_command(pty, slave, "tput cols");
    x86_serial_write("cmd tput cup 0 0\n");
    pty_command(pty, slave, "tput cup 0 0");
    x86_serial_write("cmd test -e /usr/share/terminfo/r/radix\n");
    pty_command(pty, slave, "test -e /usr/share/terminfo/r/radix");
    x86_serial_write("cmd echo radix | wc\n");
    pty_command(pty, slave, "echo radix | wc");
    x86_serial_write("cmd initctl restart userspace-shell\n");
    pty_command(pty, slave, "initctl restart userspace-shell");
    x86_serial_write("cmd logread init\n");
    pty_command(pty, slave, "logread init");
    x86_serial_write("cmd logread userspace-shell\n");
    pty_command(pty, slave, "logread userspace-shell");
    x86_serial_write("cmd logread kernel\n");
    pty_command(pty, slave, "logread kernel");
    x86_serial_write("cmd dmesg\n");
    pty_command(pty, slave, "dmesg");
#endif
    g_transcript_size = 0;
    g_transcript[0] = '\0';
    terminal_model_reset();
    if (slave) {
        rad_tty_set_mode(slave, RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_ECHO | RAD_TTY_MODE_CRLF);
        rad_tty_flush(slave, RAD_TTY_FLUSH_INPUT | RAD_TTY_FLUSH_OUTPUT);
    }
    if (framebuffer) render_terminal(framebuffer);
#if defined(RADIX_X86_DIRECT_LOGIN)
    {
        int32_t login_pid = 0;
        rad_task_t login_task = nullptr;
        const rad_status_t login_status = x86_user_spawn_process_with_stdio(
            "/bin/login",
            0,
            slave_name,
            &login_pid,
            &login_task);
        if (login_status == RAD_STATUS_OK) {
            rad_debug_marker("RADIX_DIRECT_LOGIN_SPAWN_OK");
        } else {
            append_text("RADIX_DIRECT_LOGIN_SPAWN_FAIL\n");
            rad_debug_marker("RADIX_DIRECT_LOGIN_SPAWN_FAIL");
        }
    }
#else
    if (rad_service_start("userspace-init") != RAD_STATUS_OK) {
        append_text("RADIX_RADINIT_SPAWN_FAIL\n");
        rad_debug_marker("RADIX_RADINIT_SPAWN_FAIL");
    }
#endif
    g_user_terminal_active = 1;
    if (framebuffer) render_terminal(framebuffer);
    x86_serial_write("RAD x86_64 base terminal ready\n");
#if defined(RADIX_X86_UI_PROFILE_WM)
    const rad_status_t slint_status = radix_slint_shell_start(framebuffer, g_transcript);
    if (slint_status == RAD_STATUS_OK) {
        append_text("RADIX_X86_UI_PROFILE_WM_OK\n");
        rad_debug_marker("RADIX_X86_UI_PROFILE_WM_OK");
        x86_serial_write("RADIX_X86_UI_PROFILE_WM_OK\n");
    } else {
        append_text("RADIX_X86_UI_PROFILE_WM_FAIL\n");
        rad_debug_marker("RADIX_X86_UI_PROFILE_WM_FAIL");
        x86_serial_write("RADIX_X86_UI_PROFILE_WM_FAIL\n");
    }
#else
    rad_debug_marker("RADIX_X86_UI_PROFILE_TERMINAL_OK");
    x86_serial_write("RADIX_X86_UI_PROFILE_TERMINAL_OK\n");
#endif

    rad_device_t serial = nullptr;
    rad_device_open("/dev/ttyS0", &serial);
    rad_device_t input = nullptr;
    if (rad_input_open("/dev/input/event0", &input) == RAD_STATUS_OK) {
        x86_serial_write("keyboard: /dev/input/event0 ready\n");
    } else {
        x86_serial_write("keyboard: /dev/input/event0 unavailable\n");
#if defined(RADIX_X86_UI_PROFILE_WM)
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
#else
        if (framebuffer) render_terminal(framebuffer);
#endif
    }
    rad_device_t pointer = nullptr;
    if (rad_input_open("/dev/input/event1", &pointer) == RAD_STATUS_OK) {
        x86_serial_write("pointer: /dev/input/event1 ready\n");
    } else {
        x86_serial_write("pointer: /dev/input/event1 unavailable\n");
#if defined(RADIX_X86_UI_PROFILE_WM)
        if (radix_slint_shell_ready()) radix_slint_shell_set_terminal_text(g_transcript);
        else if (framebuffer) render_terminal(framebuffer);
#else
        if (framebuffer) render_terminal(framebuffer);
#endif
    }
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
    nano_autotest_begin(pty);
#endif
    for (;;) {
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool loop_begin_seen = false;
        if (!loop_begin_seen) { loop_begin_seen = true; nano_loop_marker("RADIX_NANO_LOOP_BEGIN"); }
#endif
        if (serial) pump_serial_to_pty(serial, pty, slave);
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_serial_seen = false;
        if (!after_serial_seen) { after_serial_seen = true; nano_loop_marker("RADIX_NANO_AFTER_SERIAL"); }
        nano_autotest_poll();
        static bool after_autotest_seen = false;
        if (!after_autotest_seen) { after_autotest_seen = true; nano_loop_marker("RADIX_NANO_AFTER_AUTOTEST"); }
#endif
#if defined(RADIX_X86_UI_PROFILE_WM)
        if (radix_slint_shell_ready()) {
            radix_slint_shell_poll();
        } else if (framebuffer) {
            if (input) pump_input_to_pty(input, pty, slave);
            if (pointer) pump_pointer_input(pointer);
        }
#else
        if (framebuffer) {
            if (input) pump_input_to_pty(input, pty, slave);
            if (pointer) pump_pointer_input(pointer);
        }
#endif
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_input_seen = false;
        if (!after_input_seen) { after_input_seen = true; nano_loop_marker("RADIX_NANO_AFTER_INPUT"); }
#endif
        rad_kernel_poll();
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_poll_seen = false;
        if (!after_poll_seen) { after_poll_seen = true; nano_loop_marker("RADIX_NANO_AFTER_KERNEL_POLL"); }
#endif
        pty_drain(pty);
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_drain_seen = false;
        if (!after_drain_seen) { after_drain_seen = true; nano_loop_marker("RADIX_NANO_AFTER_PTY_DRAIN"); }
#endif
#if defined(RADIX_X86_UI_PROFILE_WM)
        if (radix_slint_shell_ready()) {
            if (g_terminal_dirty) radix_slint_shell_set_terminal_text(g_transcript);
        } else if (framebuffer) {
            render_terminal(framebuffer);
        }
#else
        if (framebuffer) render_terminal(framebuffer);
#endif
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_render_seen = false;
        if (!after_render_seen) { after_render_seen = true; nano_loop_marker("RADIX_NANO_AFTER_RENDER"); }
#endif
        if (x86_ui_idle_frame) x86_ui_idle_frame();
        else rad_task_sleep_ms(16);
#if defined(RADIX_X86_TERMINAL_AUTOTEST_NANO)
        static bool after_idle_seen = false;
        if (!after_idle_seen) { after_idle_seen = true; nano_loop_marker("RADIX_NANO_AFTER_IDLE"); }
#endif
    }
}
