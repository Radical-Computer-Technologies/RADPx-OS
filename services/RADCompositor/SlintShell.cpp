#include "SlintShell.h"

#include <radkernel/rad_display.h>
#include <radkernel/rad_input.h>
#include <radkernel/rad_pty.h>

#include "RADCompositorCore.h"

#include <memory>
#include <vector>
#include <span>
#include <string_view>
#include <utility>

#include <slint-platform.h>

#include "RADCompositor.h"

#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" void x86_ps2_poll_devices(void) __attribute__((weak));
extern "C" void x86_ui_idle_frame(void) __attribute__((weak));
extern "C" void x86_virtio_input_poll(void) __attribute__((weak));
extern "C" rad_status_t rad_shell_launch_terminal_process(void) __attribute__((weak));
extern "C" rad_status_t x86_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out) __attribute__((weak));
extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out) __attribute__((weak));
extern "C" int x86_user_wait_process(int32_t pid) __attribute__((weak));

namespace {

constexpr uint32_t MaxShellText = 8192u;
constexpr uint32_t TerminalWindowId = 1u;
constexpr uint32_t FileExplorerWindowId = 2u;
constexpr uint32_t TextEditorWindowId = 3u;
constexpr int32_t MinWindowWidth = 180;
constexpr int32_t MinWindowHeight = 112;
constexpr uint32_t MaxSurfaceWidth = 1920u;
constexpr uint32_t MaxSurfaceHeight = 1080u;

enum class SlintSurfaceRole {
    Desktop,
    Terminal,
    FileExplorer,
    TextEditor
};

enum class TerminalAppState {
    Closed = 0,
    Launching = 1,
    Running = 2
};

enum class DesktopWindowState {
    Closed = 0,
    Loading = 1,
    Running = 2
};

struct DesktopWindowBounds {
    int32_t x = 134;
    int32_t y = 110;
    int32_t width = 640;
    int32_t height = 380;
};

struct DesktopWindow {
    uint32_t id = TerminalWindowId;
    const char *title = "Terminal";
    DesktopWindowBounds bounds{};
    // Closed by default: the desktop boots with no terminal window. The user opens it
    // from the applications menu (launch_terminal), which spawns radsh and shows the window.
    DesktopWindowState state = DesktopWindowState::Closed;
    uint32_t z = 1;
    bool focused = false;
    uint32_t open_seq = 0;   // order the window was last opened, for dock ordering
};

class EmbeddedDesktopShellModel {
public:
    bool applicationsMenuOpen() const {
        return applications_menu_open_;
    }

    bool terminalOpen() const {
        return terminal_window_.state != DesktopWindowState::Closed;
    }

    bool terminalLaunching() const {
        return terminal_window_.state == DesktopWindowState::Loading;
    }

    TerminalAppState terminalState() const {
        if (terminal_window_.state == DesktopWindowState::Closed) return TerminalAppState::Closed;
        if (terminal_window_.state == DesktopWindowState::Loading) return TerminalAppState::Launching;
        return TerminalAppState::Running;
    }

    const DesktopWindow *terminalWindow() const {
        return &terminal_window_;
    }

    const DesktopWindow *fileExplorerWindow() const {
        return &file_explorer_window_;
    }

    bool fileExplorerOpen() const {
        return file_explorer_window_.state != DesktopWindowState::Closed;
    }

    bool fileExplorerLaunching() const {
        return file_explorer_window_.state == DesktopWindowState::Loading;
    }

    const DesktopWindow *textEditorWindow() const {
        return &text_editor_window_;
    }

    bool textEditorOpen() const {
        return text_editor_window_.state != DesktopWindowState::Closed;
    }

    bool textEditorLaunching() const {
        return text_editor_window_.state == DesktopWindowState::Loading;
    }

    // Id of the currently-focused open window, or 0 if none is focused. Used to route
    // keyboard events to the focused window's adapter.
    uint32_t focusedWindowId() const {
        if (terminal_window_.focused && terminal_window_.state != DesktopWindowState::Closed) return terminal_window_.id;
        if (file_explorer_window_.focused && file_explorer_window_.state != DesktopWindowState::Closed) return file_explorer_window_.id;
        if (text_editor_window_.focused && text_editor_window_.state != DesktopWindowState::Closed) return text_editor_window_.id;
        return 0;
    }

    bool focusWindow(uint32_t window_id) {
        DesktopWindow *window = windowById(window_id);
        if (!window || window->state == DesktopWindowState::Closed) return false;
        focusWindowRecord(window);
        applications_menu_open_ = false;
        return true;
    }

    bool closeWindow(uint32_t window_id) {
        DesktopWindow *window = windowById(window_id);
        if (!window) return false;
        window->state = DesktopWindowState::Closed;
        window->focused = false;
        applications_menu_open_ = false;
        return true;
    }

    // Move/resize are driven by the SCREEN-absolute cursor (cursor_x/cursor_y), not by the
    // Slint TouchArea-local delta. The drag handle lives inside the window, so as the window
    // moves/resizes the handle moves under the pointer and a TouchArea-local delta feeds back
    // on itself -> the window jitters. The screen cursor is unaffected by window movement, so
    // (cursor - cursor_at_gesture_start) is a stable delta from the bounds captured at start.
    bool moveWindow(uint32_t window_id, int32_t cursor_x, int32_t cursor_y) {
        DesktopWindow *window = windowById(window_id);
        if (!window || window->state == DesktopWindowState::Closed) return false;
        if (!move_active_ || move_window_id_ != window_id) {
            move_active_ = true;
            move_window_id_ = window_id;
            move_start_bounds_ = window->bounds;
            gesture_cursor_x_ = cursor_x;
            gesture_cursor_y_ = cursor_y;
        }
        const int32_t dx = cursor_x - gesture_cursor_x_;
        const int32_t dy = cursor_y - gesture_cursor_y_;
        window->bounds.x = clamp_range(move_start_bounds_.x + dx, 0, max_window_x(window->bounds.width));
        window->bounds.y = clamp_range(move_start_bounds_.y + dy, 38, max_window_y(window->bounds.height));
        focusWindowRecord(window);
        return true;
    }

    bool resizeWindow(uint32_t window_id, int32_t cursor_x, int32_t cursor_y) {
        DesktopWindow *window = windowById(window_id);
        if (!window || window->state == DesktopWindowState::Closed) return false;
        if (!resize_active_ || resize_window_id_ != window_id) {
            resize_active_ = true;
            resize_window_id_ = window_id;
            resize_start_bounds_ = window->bounds;
            gesture_cursor_x_ = cursor_x;
            gesture_cursor_y_ = cursor_y;
        }
        const int32_t dx = cursor_x - gesture_cursor_x_;
        const int32_t dy = cursor_y - gesture_cursor_y_;
        const int32_t max_width = clamp_min(static_cast<int32_t>(desktop_width_) - resize_start_bounds_.x, MinWindowWidth);
        const int32_t max_height = clamp_min(static_cast<int32_t>(desktop_height_) - resize_start_bounds_.y, MinWindowHeight);
        window->bounds.width = clamp_range(resize_start_bounds_.width + dx, MinWindowWidth, max_width);
        window->bounds.height = clamp_range(resize_start_bounds_.height + dy, MinWindowHeight, max_height);
        focusWindowRecord(window);
        return true;
    }

    void endPointerGesture() {
        move_active_ = false;
        resize_active_ = false;
    }

    void setDesktopExtent(uint32_t width, uint32_t height) {
        desktop_width_ = width ? width : MaxSurfaceWidth;
        desktop_height_ = height ? height : MaxSurfaceHeight;
        clampWindowToDesktop(terminal_window_);
        clampWindowToDesktop(file_explorer_window_);
        clampWindowToDesktop(text_editor_window_);
    }

    void toggleApplicationsMenu() {
        applications_menu_open_ = !applications_menu_open_;
        dock_menu_app_ = 0;
    }

    void beginTerminalLaunch() {
        // Opening a closed window starts fresh: reset to default geometry so a previous
        // drag/resize does not persist across a close/reopen.
        if (terminal_window_.state == DesktopWindowState::Closed) {
            terminal_window_.bounds = DesktopWindowBounds{ 134, 110, 640, 380 };
            terminal_window_.open_seq = next_open_seq_++;
        }
        terminal_window_.state = DesktopWindowState::Loading;
        focusTerminal();
        applications_menu_open_ = false;
    }

    void terminalReady() {
        terminal_window_.state = DesktopWindowState::Running;
        focusTerminal();
    }

    void beginFileExplorerLaunch() {
        if (file_explorer_window_.state == DesktopWindowState::Closed) {
            file_explorer_window_.bounds = DesktopWindowBounds{ 220, 150, 560, 360 };
            file_explorer_window_.open_seq = next_open_seq_++;
        }
        file_explorer_window_.state = DesktopWindowState::Loading;
        focusWindowRecord(&file_explorer_window_);
        applications_menu_open_ = false;
    }

    void fileExplorerReady() {
        file_explorer_window_.state = DesktopWindowState::Running;
        focusWindowRecord(&file_explorer_window_);
    }

    void beginTextEditorLaunch() {
        if (text_editor_window_.state == DesktopWindowState::Closed) {
            text_editor_window_.bounds = DesktopWindowBounds{ 260, 130, 700, 460 };
            text_editor_window_.open_seq = next_open_seq_++;
        }
        text_editor_window_.state = DesktopWindowState::Loading;
        focusWindowRecord(&text_editor_window_);
        applications_menu_open_ = false;
    }

    void textEditorReady() {
        text_editor_window_.state = DesktopWindowState::Running;
        focusWindowRecord(&text_editor_window_);
    }

    // Escape only dismisses transient UI (the applications menu or a dock right-click
    // dropdown). It never closes an app window: when the terminal is focused and nothing
    // transient is open, Escape is forwarded to the PTY (see dispatch_key_event) so vim and
    // other programs receive it. Returns true only when it dismissed something.
    bool handleEscape() {
        if (applications_menu_open_) {
            applications_menu_open_ = false;
            dock_menu_app_ = 0;
            return true;
        }
        if (dock_menu_app_ != 0) {
            dock_menu_app_ = 0;
            return true;
        }
        return false;
    }

    int dockMenuApp() const {
        return dock_menu_app_;
    }

    // Toggle the dock right-click "Close" dropdown for the given app (1=terminal, 2=files).
    // Only one dropdown is open at a time; opening one closes the applications menu.
    void toggleDockMenu(int app) {
        dock_menu_app_ = (dock_menu_app_ == app) ? 0 : app;
        if (dock_menu_app_ != 0) applications_menu_open_ = false;
    }

    void closeDockMenu() {
        dock_menu_app_ = 0;
    }

    // Fill `out` with the ids of currently-open windows in open order (first opened first) and
    // return the count (0..3). Drives the dock icon model + anchors the Close dropdown.
    int dockOrder(uint32_t out[3]) const {
        const DesktopWindow *open[3] = { nullptr, nullptr, nullptr };
        int n = 0;
        if (terminal_window_.state != DesktopWindowState::Closed) open[n++] = &terminal_window_;
        if (file_explorer_window_.state != DesktopWindowState::Closed) open[n++] = &file_explorer_window_;
        if (text_editor_window_.state != DesktopWindowState::Closed) open[n++] = &text_editor_window_;
        // Insertion sort by open_seq (n is at most 3) so icons render in open order.
        for (int i = 1; i < n; ++i) {
            const DesktopWindow *key = open[i];
            int j = i - 1;
            while (j >= 0 && open[j]->open_seq > key->open_seq) {
                open[j + 1] = open[j];
                --j;
            }
            open[j + 1] = key;
        }
        for (int i = 0; i < n; ++i) out[i] = open[i]->id;
        return n;
    }

private:
    static int32_t clamp_min(int32_t value, int32_t minimum) {
        return value < minimum ? minimum : value;
    }

    static int32_t clamp_range(int32_t value, int32_t minimum, int32_t maximum) {
        if (maximum < minimum) maximum = minimum;
        if (value < minimum) return minimum;
        if (value > maximum) return maximum;
        return value;
    }

    int32_t max_window_x(int32_t width) const {
        const int32_t maximum = static_cast<int32_t>(desktop_width_) - clamp_min(width, MinWindowWidth);
        return maximum > 0 ? maximum : 0;
    }

    int32_t max_window_y(int32_t height) const {
        const int32_t maximum = static_cast<int32_t>(desktop_height_) - clamp_min(height, MinWindowHeight);
        return maximum > 38 ? maximum : 38;
    }

    DesktopWindow *windowById(uint32_t id) {
        if (id == terminal_window_.id) return &terminal_window_;
        if (id == file_explorer_window_.id) return &file_explorer_window_;
        if (id == text_editor_window_.id) return &text_editor_window_;
        return nullptr;
    }

    // Exclusive focus: only one window carries `focused` at a time. Clear both records
    // first so raising one visibly de-focuses the other (border/title styling), then set
    // focus + a fresh z on the target so it composites above its sibling.
    void focusWindowRecord(DesktopWindow *window) {
        terminal_window_.focused = false;
        file_explorer_window_.focused = false;
        text_editor_window_.focused = false;
        if (!window) return;
        window->focused = true;
        window->z = next_z_++;
    }

    void focusTerminal() {
        focusWindowRecord(&terminal_window_);
    }

    void clampWindowToDesktop(DesktopWindow &window) {
        window.bounds.x = clamp_range(window.bounds.x, 0, max_window_x(window.bounds.width));
        window.bounds.y = clamp_range(window.bounds.y, 38, max_window_y(window.bounds.height));
        window.bounds.width = clamp_range(window.bounds.width, MinWindowWidth, clamp_min(static_cast<int32_t>(desktop_width_) - window.bounds.x, MinWindowWidth));
        window.bounds.height = clamp_range(window.bounds.height, MinWindowHeight, clamp_min(static_cast<int32_t>(desktop_height_) - window.bounds.y, MinWindowHeight));
    }

    bool applications_menu_open_ = false;
    int dock_menu_app_ = 0;   // 0=none, else the app-id whose dock Close dropdown is open
    uint32_t next_z_ = 2;
    uint32_t next_open_seq_ = 1;   // increments each time a window opens, for dock ordering
    uint32_t desktop_width_ = MaxSurfaceWidth;
    uint32_t desktop_height_ = MaxSurfaceHeight;
    bool move_active_ = false;
    bool resize_active_ = false;
    uint32_t move_window_id_ = 0;
    uint32_t resize_window_id_ = 0;
    int32_t gesture_cursor_x_ = 0;   // screen cursor at move/resize gesture start
    int32_t gesture_cursor_y_ = 0;
    DesktopWindowBounds move_start_bounds_{};
    DesktopWindowBounds resize_start_bounds_{};
    DesktopWindow terminal_window_{};
    DesktopWindow file_explorer_window_{
        FileExplorerWindowId,
        "Files",
        DesktopWindowBounds{ 220, 150, 560, 360 },
        DesktopWindowState::Closed,
        1,
        false
    };
    DesktopWindow text_editor_window_{
        TextEditorWindowId,
        "Editor",
        DesktopWindowBounds{ 260, 130, 700, 460 },
        DesktopWindowState::Closed,
        1,
        false
    };
};

char g_terminal_text[MaxShellText];
char g_terminal_visible_text[MaxShellText];
int32_t g_terminal_scroll_lines = 0;
int g_terminal_auto_scroll = 1;
int g_slint_started = 0;
uint64_t g_ram_budget_hint = 0;
rad_compositor_tier_t g_active_tier = RAD_COMPOSITOR_TIER_HEADLESS;
int g_tier_marker_sent = 0;

void emit_tier_marker(rad_compositor_tier_t tier) {
    g_active_tier = tier;
    if (g_tier_marker_sent) return;
    g_tier_marker_sent = 1;
    rad_debug_marker(rad_compositor_tier_marker(tier));
}
int g_boot_marker_sent = 0;
int g_input_ready_marker_sent = 0;
int g_loading_marker_sent = 0;
int g_ready_marker_sent = 0;
int g_wm_marker_sent = 0;
int g_wc_demo_marker_sent = 0;
int g_terminal_window_marker_sent = 0;
int g_menu_open_marker_sent = 0;
int g_menu_escape_marker_sent = 0;
int g_window_move_marker_sent = 0;
int g_window_resize_marker_sent = 0;
int g_terminal_close_marker_sent = 0;
int g_terminal_relaunch_marker_sent = 0;
int g_terminal_scroll_marker_sent = 0;
int g_compositor_surface_marker_sent = 0;
int g_surface_alloc_marker_sent = 0;
int g_surface_budget_marker_sent = 0;
int g_compositor_offscreen_marker_sent = 0;
int g_compositor_blit_marker_sent = 0;
int g_compositor_hit_marker_sent = 0;
int g_compositor_input_marker_sent = 0;
int g_compositor_z_marker_sent = 0;
int g_compositor_alpha_marker_sent = 0;
int g_compositor_damage_marker_sent = 0;
int g_compositor_copy_forward_marker_sent = 0;
int g_compositor_exposed_marker_sent = 0;
int g_compositor_empty_marker_sent = 0;
int g_compositor_ipc_marker_sent = 0;
int g_cursor_marker_sent = 0;
int g_cursor_move_marker_sent = 0;
int g_key_input_marker_sent = 0;
int g_framebuffer_dirty_present_marker_sent = 0;
int g_shell_smoke_actions_done = 0;
int32_t g_cursor_x = 320;
int32_t g_cursor_y = 180;
int32_t g_drawn_cursor_x = 320;
int32_t g_drawn_cursor_y = 180;
int g_cursor_dirty = 1;
rad_pty_t g_terminal_pty = nullptr;
int32_t g_terminal_pid = 0;
rad_task_t g_terminal_task = nullptr;
EmbeddedDesktopShellModel g_desktop;
uint32_t g_desktop_surface_width = 0;
uint32_t g_desktop_surface_height = 0;

// Phase 2: the desktop/terminal/present buffers are allocator-backed and sized to
// the real framebuffer (capped by the capability tier), not four static
// 1920x1080x4 arrays (~33 MB). g_surface_stride is the shared row stride in pixels;
// all four buffers use it so the compositor and present paths stay consistent.
uint32_t g_surface_stride = 0;
uint32_t g_surface_alloc_height = 0;
size_t g_surface_buffer_pixels = 0;       // g_surface_stride * g_surface_alloc_height
uint32_t *g_desktop_pixels = nullptr;
uint32_t *g_terminal_pixels = nullptr;
uint32_t *g_file_explorer_pixels = nullptr;
uint32_t *g_text_editor_pixels = nullptr;
uint32_t *g_present_front_pixels = nullptr;
uint32_t *g_present_back_pixels = nullptr;
uint32_t *g_present_front = nullptr;
uint32_t *g_present_back = nullptr;
rad_compositor_t g_compositor{};
uint32_t g_desktop_surface_id = 0;
uint32_t g_terminal_surface_id = 0;
uint32_t g_file_explorer_surface_id = 0;
uint32_t g_text_editor_surface_id = 0;

// File explorer listing state. g_explorer_path is the directory currently shown (hardcoded
// to "/" for now; kept as a buffer so future navigation can rewrite it). g_explorer_entries
// is the newline-joined listing pushed into the explorer window's `entries` property.
char g_explorer_path[256] = "/";
char g_explorer_entries[MaxShellText];

// Parallel record of the rows currently shown in the explorer, so a navigate(index) click
// from the Slint Repeater can resolve back to a name/kind without reading the model.
struct ExplorerRow {
    char name[128];
    bool is_dir;
};
ExplorerRow g_explorer_rows[256];
int g_explorer_row_count = 0;

// Text editor state. g_editor_path is the file currently loaded/saved; g_editor_document is
// the latest text (kept in sync from the TextEdit via text_changed so Save persists edits).
char g_editor_path[256] = "/tmp/untitled.txt";
char g_editor_document[65536];

slint::ComponentHandle<RadDesktopSurface> *g_desktop_shell = nullptr;
slint::ComponentHandle<RadTerminalSurface> *g_terminal_shell = nullptr;
slint::ComponentHandle<RadFileExplorerSurface> *g_file_explorer_shell = nullptr;
slint::ComponentHandle<RadTextEditorSurface> *g_text_editor_shell = nullptr;

void marker_once(int *flag, const char *marker);
int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum);
void request_terminal_surface_redraw(void);

slint::SharedString shared_string(const char *text) {
    if (!text) text = "";
    return slint::SharedString(std::string_view(text, strlen(text)));
}

int32_t terminal_visible_rows(void) {
    const DesktopWindow *window = g_desktop.terminalWindow();
    const int32_t height = window ? window->bounds.height : 380;
    const int32_t text_height = height - 46;
    if (text_height <= 0) return 1;
    const int32_t rows = text_height / 15;
    return rows > 0 ? rows : 1;
}

int32_t terminal_line_count(const char *text) {
    if (!text || !*text) return 1;
    int32_t lines = 1;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n' && p[1] != '\0') ++lines;
    }
    return lines;
}

const char *terminal_line_start(const char *text, int32_t line) {
    if (!text || line <= 0) return text ? text : "";
    int32_t current = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == '\n') {
            ++current;
            if (current >= line) return p + 1;
        }
    }
    return "";
}

int32_t terminal_max_scroll_lines(void) {
    const int32_t total = terminal_line_count(g_terminal_text);
    const int32_t visible = terminal_visible_rows();
    return total > visible ? total - visible : 0;
}

void update_terminal_visible_text(void) {
    if (g_terminal_auto_scroll) g_terminal_scroll_lines = 0;
    const int32_t max_scroll = terminal_max_scroll_lines();
    g_terminal_scroll_lines = clamp_i32(g_terminal_scroll_lines, 0, max_scroll);
    const int32_t start_line = clamp_i32(terminal_line_count(g_terminal_text) - terminal_visible_rows() - g_terminal_scroll_lines, 0, terminal_line_count(g_terminal_text));
    const char *start = terminal_line_start(g_terminal_text, start_line);
    strncpy(g_terminal_visible_text, start, sizeof(g_terminal_visible_text) - 1u);
    g_terminal_visible_text[sizeof(g_terminal_visible_text) - 1u] = '\0';
}

void scroll_terminal(int32_t lines) {
    if (lines == 0) return;
    g_terminal_auto_scroll = 0;
    g_terminal_scroll_lines = clamp_i32(g_terminal_scroll_lines + lines, 0, terminal_max_scroll_lines());
    if (g_terminal_scroll_lines == 0) g_terminal_auto_scroll = 1;
    update_terminal_visible_text();
    if (g_terminal_shell) (*g_terminal_shell)->set_terminal(shared_string(g_terminal_visible_text));
    marker_once(&g_terminal_scroll_marker_sent, "RAD_SLINT_TERMINAL_SCROLL_OK");
    request_terminal_surface_redraw();
}

// --- minimal terminal line discipline ------------------------------------------------
// The window renders raw pty bytes, so radsh's control codes (colors, CR line-redraws,
// backspace) have to be interpreted here or they show as garbage and never clear a line.
// State: committed lines (each ending '\n') + the in-progress current line with a cursor.
char g_term_committed[MaxShellText];
size_t g_term_committed_len = 0;
char g_term_line[1024];
size_t g_term_line_len = 0;
size_t g_term_cursor = 0;
int g_term_esc = 0;          // 0 normal, 1 saw ESC, 2 inside CSI
char g_term_csi[16];
size_t g_term_csi_len = 0;

bool term_line_is_marker() {
    // radsh writes self-test markers (RAD_TERMINAL_ANSI_OK, ...) to stdout; drop them from
    // the visible terminal. They still reach the serial log via the pty mirror for the smoke.
    if (g_term_line_len < 5 || g_term_line[0] != 'R' || g_term_line[1] != 'A'
        || g_term_line[2] != 'D' || g_term_line[3] != '_') return false;
    for (size_t i = 0; i < g_term_line_len; ++i) {
        const char ch = g_term_line[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

void term_commit_line() {
    if (!term_line_is_marker()) {
        const size_t need = g_term_line_len + 1u;
        if (need < sizeof(g_term_committed)) {
            while (g_term_committed_len + need >= sizeof(g_term_committed) && g_term_committed_len > 0) {
                size_t k = 0;
                while (k < g_term_committed_len && g_term_committed[k] != '\n') ++k;
                if (k < g_term_committed_len) ++k;
                if (k == 0) { g_term_committed_len = 0; break; }
                memmove(g_term_committed, g_term_committed + k, g_term_committed_len - k);
                g_term_committed_len -= k;
            }
            memcpy(g_term_committed + g_term_committed_len, g_term_line, g_term_line_len);
            g_term_committed_len += g_term_line_len;
            g_term_committed[g_term_committed_len++] = '\n';
        }
    }
    g_term_line_len = 0;
    g_term_cursor = 0;
}

void term_put_visible(char c) {
    if (g_term_cursor < sizeof(g_term_line) - 1u) {
        g_term_line[g_term_cursor++] = c;
        if (g_term_cursor > g_term_line_len) g_term_line_len = g_term_cursor;
    }
}

void term_feed_byte(char c) {
    if (g_term_esc == 1) {                       // after ESC: only CSI ("[") is handled
        g_term_esc = (c == '[') ? 2 : 0;
        if (g_term_esc == 2) g_term_csi_len = 0;
        return;
    }
    if (g_term_esc == 2) {                        // collecting CSI params until a final letter
        if ((c >= '0' && c <= '9') || c == ';') {
            if (g_term_csi_len < sizeof(g_term_csi) - 1u) g_term_csi[g_term_csi_len++] = c;
            return;
        }
        if (c == 'K') {                           // erase-in-line: 2=whole line, else to end
            if (g_term_csi_len >= 1 && g_term_csi[0] == '2') { g_term_line_len = 0; g_term_cursor = 0; }
            else { g_term_line_len = g_term_cursor; }
        }
        // 'm' (color), 'C'/'D' (cursor), 'J', etc. are ignored.
        g_term_esc = 0;
        return;
    }
    switch (c) {
    case '\x1b': g_term_esc = 1; return;
    case '\n': term_commit_line(); return;
    case '\r': g_term_cursor = 0; return;
    case '\b': if (g_term_cursor > 0) --g_term_cursor; return;
    case '\t': do { term_put_visible(' '); } while ((g_term_cursor % 8u) != 0u && g_term_cursor < sizeof(g_term_line) - 1u); return;
    default:
        if (static_cast<unsigned char>(c) < 0x20u) return;  // drop other control bytes
        term_put_visible(c);
        return;
    }
}

void term_rebuild_visible() {
    size_t n = g_term_committed_len;
    if (n > sizeof(g_terminal_text) - 1u) n = sizeof(g_terminal_text) - 1u;
    memcpy(g_terminal_text, g_term_committed, n);
    size_t line_copy = g_term_line_len;
    if (n + line_copy > sizeof(g_terminal_text) - 1u) line_copy = sizeof(g_terminal_text) - 1u - n;
    memcpy(g_terminal_text + n, g_term_line, line_copy);
    g_terminal_text[n + line_copy] = '\0';
}

void copy_terminal_text(const char *text) {
    g_term_committed_len = 0;
    g_term_line_len = 0;
    g_term_cursor = 0;
    g_term_esc = 0;
    g_term_csi_len = 0;
    if (text) {
        for (const char *p = text; *p; ++p) term_feed_byte(*p);
    }
    term_rebuild_visible();
    update_terminal_visible_text();
}

void append_terminal_text(const char *text, size_t size) {
    if (!text || !size) return;
    for (size_t i = 0; i < size; ++i) term_feed_byte(text[i]);
    term_rebuild_visible();
    update_terminal_visible_text();
}

void marker_once(int *flag, const char *marker) {
    if (!flag || *flag || !marker) return;
    *flag = 1;
    rad_debug_marker(marker);
}

int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

rad_compositor_rect_t cursor_rect_at(int32_t x, int32_t y) {
    rad_compositor_rect_t rect{};
    rect.x = x;
    rect.y = y;
    rect.width = 18;
    rect.height = 24;
    if (rect.x + rect.width > static_cast<int32_t>(g_desktop_surface_width)) rect.width = static_cast<int32_t>(g_desktop_surface_width) - rect.x;
    if (rect.y + rect.height > static_cast<int32_t>(g_desktop_surface_height)) rect.height = static_cast<int32_t>(g_desktop_surface_height) - rect.y;
    if (rect.width < 1) rect.width = 1;
    if (rect.height < 1) rect.height = 1;
    return rect;
}

void queue_cursor_damage(void) {
    if (!g_cursor_dirty || !g_desktop_surface_id) return;
    rad_compositor_rect_t previous = cursor_rect_at(g_drawn_cursor_x, g_drawn_cursor_y);
    rad_compositor_rect_t current = cursor_rect_at(g_cursor_x, g_cursor_y);
    if (rad_compositor_queue_damage(&g_compositor, g_desktop_surface_id, &previous, RAD_COMPOSITOR_DAMAGE_EXPOSED) == RAD_STATUS_OK) {
        marker_once(&g_compositor_damage_marker_sent, "RAD_COMPOSITOR_DAMAGE_QUEUE_OK");
        marker_once(&g_compositor_exposed_marker_sent, "RAD_COMPOSITOR_EXPOSED_DAMAGE_OK");
    }
    if (rad_compositor_queue_damage(&g_compositor, g_desktop_surface_id, &current, RAD_COMPOSITOR_DAMAGE_EXPOSED) == RAD_STATUS_OK) {
        marker_once(&g_compositor_damage_marker_sent, "RAD_COMPOSITOR_DAMAGE_QUEUE_OK");
        marker_once(&g_compositor_exposed_marker_sent, "RAD_COMPOSITOR_EXPOSED_DAMAGE_OK");
    }
}

void draw_cursor(uint32_t *pixels, uint32_t stride_pixels) {
    if (!pixels || stride_pixels < g_desktop_surface_width) return;
    static constexpr uint8_t shape[16] = {
        0b10000000,
        0b11000000,
        0b11100000,
        0b11110000,
        0b11111000,
        0b11111100,
        0b11111110,
        0b11111100,
        0b11101100,
        0b11001110,
        0b10000110,
        0b00000111,
        0b00000011,
        0b00000011,
        0b00000000,
        0b00000000,
    };
    for (int32_t row = 0; row < 16; ++row) {
        const int32_t y = g_cursor_y + row;
        if (y < 0 || y >= static_cast<int32_t>(g_desktop_surface_height)) continue;
        for (int32_t col = 0; col < 8; ++col) {
            const int32_t x = g_cursor_x + col;
            if (x < 0 || x >= static_cast<int32_t>(g_desktop_surface_width)) continue;
            if ((shape[row] & (0x80u >> col)) == 0) continue;
            const int edge = col == 0 || row == 0 || (col > 0 && (shape[row] & (0x80u >> (col - 1))) == 0);
            pixels[static_cast<size_t>(y) * stride_pixels + static_cast<size_t>(x)] = edge ? 0xff020617u : 0xffffffffu;
        }
    }
    marker_once(&g_cursor_marker_sent, "RAD_SLINT_CURSOR_OK");
    g_drawn_cursor_x = g_cursor_x;
    g_drawn_cursor_y = g_cursor_y;
    g_cursor_dirty = 0;
}

void update_cursor_position(const rad_input_event_t& event) {
    if (event.type != RAD_INPUT_EVENT_POINTER_MOTION && event.type != RAD_INPUT_EVENT_POINTER_BUTTON && event.type != RAD_INPUT_EVENT_POINTER_SCROLL) return;
    const int32_t old_x = g_cursor_x;
    const int32_t old_y = g_cursor_y;
    g_cursor_x = clamp_i32(event.x, 0, static_cast<int32_t>(g_desktop_surface_width ? g_desktop_surface_width - 1u : 0u));
    g_cursor_y = clamp_i32(event.y, 0, static_cast<int32_t>(g_desktop_surface_height ? g_desktop_surface_height - 1u : 0u));
    if (g_cursor_x != old_x || g_cursor_y != old_y || event.type == RAD_INPUT_EVENT_POINTER_BUTTON) {
        g_cursor_dirty = 1;
        if (g_cursor_x != old_x || g_cursor_y != old_y) {
            marker_once(&g_cursor_move_marker_sent, "RAD_SLINT_CURSOR_MOVE_OK");
        }
    }
}

uint32_t clamp_surface_extent(uint32_t value, uint32_t maximum) {
    if (value == 0) return 1u;
    return value > maximum ? maximum : value;
}

// Per-tier resolution caps. LEAN targets (Pi Zero 2 W, 512 MB) cap at 720p so the
// four surface buffers stay small; FULL uses the max supported surface.
void tier_surface_caps(rad_compositor_tier_t tier, uint32_t *max_w, uint32_t *max_h) {
    if (tier == RAD_COMPOSITOR_TIER_LEAN) { *max_w = 1280u; *max_h = 720u; }
    else { *max_w = MaxSurfaceWidth; *max_h = MaxSurfaceHeight; }
}

// Allocate the four surface/present buffers at the given stride and height from the
// kernel allocator. Returns false (freeing any partial allocation) on failure.
bool alloc_surface_buffers(uint32_t stride, uint32_t height) {
    const size_t count = static_cast<size_t>(stride) * static_cast<size_t>(height);
    const size_t bytes = count * sizeof(uint32_t);
    g_desktop_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    g_terminal_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    g_file_explorer_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    g_text_editor_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    g_present_front_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    g_present_back_pixels = static_cast<uint32_t*>(rad_memory_alloc(bytes));
    if (!g_desktop_pixels || !g_terminal_pixels || !g_file_explorer_pixels || !g_text_editor_pixels || !g_present_front_pixels || !g_present_back_pixels) {
        rad_memory_free(g_desktop_pixels);
        rad_memory_free(g_terminal_pixels);
        rad_memory_free(g_file_explorer_pixels);
        rad_memory_free(g_text_editor_pixels);
        rad_memory_free(g_present_front_pixels);
        rad_memory_free(g_present_back_pixels);
        g_desktop_pixels = nullptr;
        g_terminal_pixels = nullptr;
        g_file_explorer_pixels = nullptr;
        g_text_editor_pixels = nullptr;
        g_present_front_pixels = nullptr;
        g_present_back_pixels = nullptr;
        return false;
    }
    g_surface_stride = stride;
    g_surface_alloc_height = height;
    g_surface_buffer_pixels = count;
    g_present_front = g_present_front_pixels;
    g_present_back = g_present_back_pixels;
    return true;
}

// ---------------------------------------------------------------------------
// Client/server surface ownership + per-surface input routing.
//
// A userland process creates a compositor surface over shared memory via the
// /dev/compositor0 ioctls. The kernel needs to (a) remember which process owns
// each client surface so it can tear the surface down when that process exits,
// and (b) deliver input that hit-tests / focuses onto a client surface to that
// process (which polls it back out with RAD_DEVICE_IOCTL_COMPOSITOR_POLL_INPUT),
// rather than into an in-kernel Slint adapter (client surfaces have none).
// ---------------------------------------------------------------------------
#ifndef RAD_IPC_INPUT_RING
#define RAD_IPC_INPUT_RING 64u
#endif

struct IpcSurfaceRecord {
    uint32_t surface_id;
    int32_t owner_pid;
    int used;
    rad_input_event_t ring[RAD_IPC_INPUT_RING];
    uint32_t ring_head;
    uint32_t ring_count;
};

IpcSurfaceRecord g_ipc_surfaces[RAD_COMPOSITOR_MAX_SURFACES];

IpcSurfaceRecord *ipc_find(uint32_t surface_id) {
    if (surface_id == 0) return nullptr;
    for (auto &record : g_ipc_surfaces) {
        if (record.used && record.surface_id == surface_id) return &record;
    }
    return nullptr;
}

bool ipc_is_surface(uint32_t surface_id) { return ipc_find(surface_id) != nullptr; }

IpcSurfaceRecord *ipc_alloc(uint32_t surface_id, int32_t owner_pid) {
    for (auto &record : g_ipc_surfaces) {
        if (record.used) continue;
        record = IpcSurfaceRecord{};
        record.used = 1;
        record.surface_id = surface_id;
        record.owner_pid = owner_pid;
        return &record;
    }
    return nullptr;
}

void ipc_push_input(IpcSurfaceRecord *record, const rad_input_event_t &event) {
    if (!record) return;
    if (record->ring_count == RAD_IPC_INPUT_RING) {
        // Drop the oldest event so live input never stalls behind a slow client.
        record->ring_head = (record->ring_head + 1) % RAD_IPC_INPUT_RING;
        --record->ring_count;
    }
    const uint32_t slot = (record->ring_head + record->ring_count) % RAD_IPC_INPUT_RING;
    record->ring[slot] = event;
    ++record->ring_count;
}

int ipc_pop_input(uint32_t surface_id, rad_input_event_t *out) {
    IpcSurfaceRecord *record = ipc_find(surface_id);
    if (!record || record->ring_count == 0) return 0;
    if (out) *out = record->ring[record->ring_head];
    record->ring_head = (record->ring_head + 1) % RAD_IPC_INPUT_RING;
    --record->ring_count;
    return 1;
}

// Fetch a live surface's screen bounds from the compositor by id (for exposed
// damage when a client surface is torn down).
bool compositor_surface_bounds(uint32_t surface_id, rad_compositor_rect_t *out) {
    rad_compositor_surface_info_t infos[RAD_COMPOSITOR_MAX_SURFACES];
    const size_t n = rad_compositor_list_surfaces(&g_compositor, infos, RAD_COMPOSITOR_MAX_SURFACES);
    for (size_t i = 0; i < n; ++i) {
        if (infos[i].id == surface_id) {
            if (out) *out = infos[i].bounds;
            return true;
        }
    }
    return false;
}

// Destroy a client surface: queue exposed damage over its footprint (while it
// still exists, so the region repaints), then remove it and drop its owner slot.
void ipc_destroy_surface(uint32_t surface_id) {
    rad_compositor_rect_t bounds{};
    if (compositor_surface_bounds(surface_id, &bounds)) {
        rad_compositor_queue_damage(&g_compositor, surface_id, &bounds, RAD_COMPOSITOR_DAMAGE_EXPOSED);
    }
    rad_compositor_destroy_surface(&g_compositor, surface_id);
    if (IpcSurfaceRecord *record = ipc_find(surface_id)) {
        record->used = 0;
        record->surface_id = 0;
    }
}

// Is a pid a live (non-zombie) process? A pid that has exited and been reaped no
// longer appears in the process list at all, so "absent" also means dead.
bool ipc_owner_alive(int32_t pid) {
    rad_process_info_t procs[64];
    const size_t n = rad_process_list(procs, 64);
    for (size_t i = 0; i < n; ++i) {
        if (procs[i].pid == pid) {
            return procs[i].state != RAD_PROCESS_ZOMBIE && !procs[i].exited;
        }
    }
    return false;
}

// Reap client surfaces whose owning process has exited. Called from the shell
// tick; returns the number of surfaces torn down.
int ipc_reap_dead_owners() {
    int reaped = 0;
    for (auto &record : g_ipc_surfaces) {
        if (!record.used || record.owner_pid <= 0) continue;
        if (ipc_owner_alive(record.owner_pid)) continue;
        ipc_destroy_surface(record.surface_id);
        ++reaped;
    }
    return reaped;
}

rad_status_t compositor_device_ioctl(void*, uint32_t request, void *argument) {
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_CREATE_SURFACE) {
        auto *surface = static_cast<rad_compositor_ipc_surface_t*>(argument);
        if (!surface || surface->size < sizeof(*surface) || surface->width == 0 || surface->height == 0) return RAD_STATUS_INVALID_ARGUMENT;
        uint32_t *pixels = static_cast<uint32_t*>(rad_shm_kernel_pointer(surface->shm_fd));
        if (!pixels) return RAD_STATUS_INVALID_ARGUMENT;
        rad_compositor_surface_config_t config{};
        config.size = sizeof(config);
        config.app_id = "rad.ipc";
        config.title = "IPC Surface";
        config.x = surface->x;
        config.y = surface->y;
        config.width = static_cast<int32_t>(surface->width);
        config.height = static_cast<int32_t>(surface->height);
        config.z = surface->z;
        config.flags = surface->flags;
        config.pixels = pixels;
        config.stride_pixels = surface->stride_pixels;
        const rad_status_t status = rad_compositor_create_surface(&g_compositor, &config, &surface->surface_id);
        if (status == RAD_STATUS_OK) {
            ipc_alloc(surface->surface_id, rad_process_current_pid());
            marker_once(&g_compositor_ipc_marker_sent, "RAD_COMPOSITOR_IPC_SURFACE_OK");
        }
        return status;
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_DESTROY_SURFACE) {
        auto *surface = static_cast<rad_compositor_ipc_surface_t*>(argument);
        if (!surface || surface->size < sizeof(*surface)) return RAD_STATUS_INVALID_ARGUMENT;
        if (!ipc_find(surface->surface_id)) return RAD_STATUS_NOT_FOUND;
        ipc_destroy_surface(surface->surface_id);
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_SET_BOUNDS) {
        auto *surface = static_cast<rad_compositor_ipc_surface_t*>(argument);
        if (!surface || surface->size < sizeof(*surface)) return RAD_STATUS_INVALID_ARGUMENT;
        if (!ipc_find(surface->surface_id)) return RAD_STATUS_NOT_FOUND;
        rad_compositor_rect_t bounds{};
        bounds.x = surface->x;
        bounds.y = surface->y;
        bounds.width = static_cast<int32_t>(surface->width);
        bounds.height = static_cast<int32_t>(surface->height);
        return rad_compositor_set_surface_bounds(&g_compositor, surface->surface_id, &bounds);
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_FOCUS) {
        auto *surface = static_cast<rad_compositor_ipc_surface_t*>(argument);
        if (!surface || surface->size < sizeof(*surface)) return RAD_STATUS_INVALID_ARGUMENT;
        if (!ipc_find(surface->surface_id)) return RAD_STATUS_NOT_FOUND;
        return rad_compositor_focus_surface(&g_compositor, surface->surface_id);
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_POLL_INPUT) {
        auto *poll = static_cast<rad_compositor_ipc_input_t*>(argument);
        if (!poll || poll->size < sizeof(*poll)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_input_event_t event{};
        poll->has_event = ipc_pop_input(poll->surface_id, &event) ? 1u : 0u;
        if (poll->has_event) poll->event = event;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_COMPOSITOR_QUEUE_DAMAGE) {
        auto *damage = static_cast<rad_compositor_ipc_damage_t*>(argument);
        if (!damage || damage->size < sizeof(*damage)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_compositor_rect_t rect{};
        rect.x = damage->x;
        rect.y = damage->y;
        rect.width = damage->width;
        rect.height = damage->height;
        const rad_status_t status = rad_compositor_queue_damage(&g_compositor, damage->surface_id, &rect, damage->flags);
        if (status == RAD_STATUS_OK) {
            marker_once(&g_compositor_damage_marker_sent, "RAD_COMPOSITOR_DAMAGE_QUEUE_OK");
            if (damage->flags & RAD_COMPOSITOR_DAMAGE_EXPOSED) {
                marker_once(&g_compositor_exposed_marker_sent, "RAD_COMPOSITOR_EXPOSED_DAMAGE_OK");
            }
        }
        return status;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

void register_compositor_device() {
    rad_device_ops_t ops{};
    ops.ioctl = compositor_device_ioctl;
    rad_device_register("/dev/compositor0", RAD_DEVICE_COMPOSITOR, &ops);
}

void drain_terminal_pty(void) {
    if (!g_terminal_pty || !g_terminal_shell) return;
    char buffer[256];
    size_t count = 0;
    int changed = 0;
    while (rad_pty_read_master(g_terminal_pty, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
        append_terminal_text(buffer, count);
        changed = 1;
    }
    if (changed) {
        (*g_terminal_shell)->set_terminal(shared_string(g_terminal_visible_text));
        marker_once(&g_ready_marker_sent, "RAD_SLINT_TERMINAL_READY_OK");
    }
}

void write_terminal_input(const char *text, size_t size) {
    const DesktopWindow *window = g_desktop.terminalWindow();
    if (!g_terminal_pty || !text || !size || !window || window->state == DesktopWindowState::Closed || !window->focused) return;
    size_t written = 0;
    rad_pty_write_master(g_terminal_pty, text, size, &written);
    if (written > 0) marker_once(&g_key_input_marker_sent, "RAD_SLINT_KEY_INPUT_OK");
}

void set_shell_state(const char *status = nullptr);

int32_t bounded_drag_delta(float value) {
    if (value != value) return 0;
    if (value > 4096.0f) return 4096;
    if (value < -4096.0f) return -4096;
    return static_cast<int32_t>(value);
}

class RadSlintWindowAdapter final : public slint::platform::WindowAdapter {
public:
    RadSlintWindowAdapter(SlintSurfaceRole role, uint32_t surface_id, uint32_t *pixels, uint32_t width, uint32_t height, uint32_t stride_pixels)
        : role_(role),
          surface_id_(surface_id),
          pixels_(pixels),
          stride_pixels_(stride_pixels),
          renderer_(slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer),
          size_({ width, height }) {}

    slint::platform::AbstractRenderer& renderer() override {
        return renderer_;
    }

    slint::PhysicalSize size() override {
        return size_;
    }

    void set_size(slint::PhysicalSize) override {
        dispatch_resize();
    }

    void set_visible(bool visible) override {
        if (visible) {
            dispatch_resize();
            needs_redraw_ = true;
        }
    }

    void request_redraw() override {
        needs_redraw_ = true;
    }

    uint32_t surface_id() const {
        return surface_id_;
    }

    SlintSurfaceRole role() const {
        return role_;
    }

    void update_size(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || width > MaxSurfaceWidth || height > MaxSurfaceHeight) return;
        if (size_.width == width && size_.height == height) return;
        size_ = slint::PhysicalSize({ width, height });
        dispatch_resize();
        request_redraw();
    }

    rad_status_t render_if_needed(bool *rendered) {
        if (rendered) *rendered = false;
        if (!needs_redraw_) return RAD_STATUS_OK;
        needs_redraw_ = false;
        if (!pixels_ || stride_pixels_ < size_.width) return RAD_STATUS_NOT_SUPPORTED;

        std::size_t dirty_min_x = size_.width;
        std::size_t dirty_min_y = size_.height;
        std::size_t dirty_max_x = 0;
        std::size_t dirty_max_y = 0;
        renderer_.render_by_line<slint::Rgb8Pixel>(
            [&](std::size_t line, std::size_t begin, std::size_t end, auto render_line) {
                if (line >= size_.height || begin >= end || end > size_.width) return;
                const std::size_t count = end - begin;
                if (count > sizeof(line_buffer_) / sizeof(line_buffer_[0])) return;
                if (begin < dirty_min_x) dirty_min_x = begin;
                if (line < dirty_min_y) dirty_min_y = line;
                if (end > dirty_max_x) dirty_max_x = end;
                if (line + 1u > dirty_max_y) dirty_max_y = line + 1u;
                std::span<slint::Rgb8Pixel> span(line_buffer_, count);
                render_line(span);
                auto *dst = pixels_ + (line * stride_pixels_) + begin;
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& src = line_buffer_[i];
                    dst[i] = 0xff000000u
                        | (uint32_t(src.r) << 16u)
                        | (uint32_t(src.g) << 8u)
                        | uint32_t(src.b);
                }
            });
        if (dirty_min_x < dirty_max_x && dirty_min_y < dirty_max_y) {
            rad_compositor_rect_t damage{};
            damage.x = static_cast<int32_t>(dirty_min_x);
            damage.y = static_cast<int32_t>(dirty_min_y);
            damage.width = static_cast<int32_t>(dirty_max_x - dirty_min_x);
            damage.height = static_cast<int32_t>(dirty_max_y - dirty_min_y);
            rad_compositor_queue_damage(&g_compositor, surface_id_, &damage, 0);
        }
        marker_once(&g_compositor_offscreen_marker_sent, "RAD_COMPOSITOR_OFFSCREEN_RENDER_OK");
        if (rendered) *rendered = true;
        return RAD_STATUS_OK;
    }

    void dispatch_input_event(const rad_input_event_t& event, int32_t local_x = 0, int32_t local_y = 0) {
        switch (event.type) {
        case RAD_INPUT_EVENT_KEY:
            dispatch_key_event(event);
            break;
        case RAD_INPUT_EVENT_POINTER_MOTION:
            window().dispatch_pointer_move_event(pointer_position(event, local_x, local_y));
            break;
        case RAD_INPUT_EVENT_POINTER_BUTTON:
            if (event.pressed) {
                window().dispatch_pointer_press_event(pointer_position(event, local_x, local_y), pointer_button(event.code));
            } else {
                window().dispatch_pointer_release_event(pointer_position(event, local_x, local_y), pointer_button(event.code));
            }
            request_redraw();
            break;
        case RAD_INPUT_EVENT_POINTER_SCROLL:
            if (role_ == SlintSurfaceRole::Terminal && event.scroll_y != 0) {
                scroll_terminal(event.scroll_y > 0 ? 3 : -3);
            }
            window().dispatch_pointer_scroll_event(pointer_position(event, local_x, local_y),
                static_cast<float>(event.scroll_x),
                static_cast<float>(event.scroll_y));
            request_redraw();
            break;
        default:
            break;
        }
    }

private:
    void dispatch_resize() {
        window().dispatch_resize_event(slint::LogicalSize({ float(size_.width), float(size_.height) }));
    }

    slint::SharedString key_text(const rad_input_event_t& event) {
        switch (event.code) {
        case RAD_INPUT_KEY_ESCAPE: return shared_string("\x1b");
        case RAD_INPUT_KEY_ENTER: return shared_string("\r");
        case RAD_INPUT_KEY_BACKSPACE: return shared_string("\x08");
        case RAD_INPUT_KEY_TAB: return shared_string("\t");
        default:
            break;
        }
        if (event.codepoint >= 0x20u && event.codepoint <= 0x7eu) {
            char text[2] = { static_cast<char>(event.codepoint), '\0' };
            return shared_string(text);
        }
        return shared_string("");
    }

    void dispatch_key_event(const rad_input_event_t& event) {
        if (role_ == SlintSurfaceRole::Terminal && event.pressed) {
            if (event.code == RAD_INPUT_KEY_PAGE_UP) {
                scroll_terminal(terminal_visible_rows());
                return;
            }
            if (event.code == RAD_INPUT_KEY_PAGE_DOWN) {
                scroll_terminal(-terminal_visible_rows());
                return;
            }
        }
        const slint::SharedString text = key_text(event);
        if (text.empty()) return;
        // Only the terminal window translates keys to PTY bytes. The editor (and any other
        // Slint-driven window) must receive keys through Slint's key dispatch below so its
        // TextEdit edits text -- forwarding them to the PTY here would type into radsh instead.
        if (role_ == SlintSurfaceRole::Terminal && event.pressed) {
            char input[2] = {};
            size_t input_size = 0;
            if (event.code == RAD_INPUT_KEY_ENTER) {
                input[0] = '\n';
                input_size = 1;
            } else if (event.code == RAD_INPUT_KEY_ESCAPE) {
                // Forward Escape to the PTY as the ESC byte so vim and other programs
                // receive it. The WM no longer closes the window on Escape.
                input[0] = '\x1b';
                input_size = 1;
            } else if (event.code == RAD_INPUT_KEY_BACKSPACE) {
                input[0] = '\b';
                input_size = 1;
            } else if (event.code == RAD_INPUT_KEY_TAB) {
                input[0] = '\t';
                input_size = 1;
            } else if (event.codepoint >= 0x20u && event.codepoint <= 0x7eu) {
                input[0] = static_cast<char>(event.codepoint);
                input_size = 1;
            }
            write_terminal_input(input, input_size);
        }
        if (event.pressed) {
            if (event.repeat) {
                window().dispatch_key_press_repeat_event(text);
            } else {
                window().dispatch_key_press_event(text);
            }
        } else {
            window().dispatch_key_release_event(text);
        }
        request_redraw();
    }

    slint::LogicalPosition pointer_position(const rad_input_event_t&, int32_t local_x, int32_t local_y) const {
        float x = static_cast<float>(local_x);
        float y = static_cast<float>(local_y);
        if (x < 0.0f) x = 0.0f;
        if (y < 0.0f) y = 0.0f;
        if (x >= static_cast<float>(size_.width)) x = static_cast<float>(size_.width - 1u);
        if (y >= static_cast<float>(size_.height)) y = static_cast<float>(size_.height - 1u);
        return slint::LogicalPosition(slint::Point<float>{ x, y });
    }

    slint::PointerEventButton pointer_button(uint32_t button) const {
        if (button == RAD_INPUT_POINTER_BUTTON_RIGHT) return slint::PointerEventButton::Right;
        if (button == RAD_INPUT_POINTER_BUTTON_MIDDLE) return slint::PointerEventButton::Middle;
        return slint::PointerEventButton::Left;
    }

    SlintSurfaceRole role_ = SlintSurfaceRole::Desktop;
    uint32_t surface_id_ = 0;
    uint32_t *pixels_ = nullptr;
    uint32_t stride_pixels_ = 0;
    slint::platform::SoftwareRenderer renderer_;
    slint::PhysicalSize size_{};
    bool needs_redraw_ = true;
    slint::Rgb8Pixel line_buffer_[4096]{};
};

class RadSlintPlatform final : public slint::platform::Platform {
public:
    RadSlintPlatform(rad_framebuffer_t framebuffer, const rad_framebuffer_info_t& info)
        : framebuffer_(framebuffer), info_(info) {
        if (rad_input_open("/dev/input/event0", &keyboard_) != RAD_STATUS_OK) keyboard_ = nullptr;
        if (rad_input_open("/dev/input/event1", &pointer_) != RAD_STATUS_OK) pointer_ = nullptr;
        started_ms_ = rad_time_millis();
    }

    void set_next_role(SlintSurfaceRole role) {
        next_role_ = role;
    }

    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
        if (next_role_ == SlintSurfaceRole::Terminal) {
            const DesktopWindow *terminal = g_desktop.terminalWindow();
            const uint32_t width = terminal && terminal->bounds.width > 0 ? static_cast<uint32_t>(terminal->bounds.width) : 640u;
            const uint32_t height = terminal && terminal->bounds.height > 0 ? static_cast<uint32_t>(terminal->bounds.height) : 380u;
            auto window = std::make_unique<RadSlintWindowAdapter>(SlintSurfaceRole::Terminal, g_terminal_surface_id, g_terminal_pixels, width, height, g_surface_stride);
            terminal_window_ = window.get();
            next_role_ = SlintSurfaceRole::Desktop;
            return window;
        }
        if (next_role_ == SlintSurfaceRole::FileExplorer) {
            const DesktopWindow *explorer = g_desktop.fileExplorerWindow();
            const uint32_t width = explorer && explorer->bounds.width > 0 ? static_cast<uint32_t>(explorer->bounds.width) : 560u;
            const uint32_t height = explorer && explorer->bounds.height > 0 ? static_cast<uint32_t>(explorer->bounds.height) : 360u;
            auto window = std::make_unique<RadSlintWindowAdapter>(SlintSurfaceRole::FileExplorer, g_file_explorer_surface_id, g_file_explorer_pixels, width, height, g_surface_stride);
            file_explorer_window_ = window.get();
            next_role_ = SlintSurfaceRole::Desktop;
            return window;
        }
        if (next_role_ == SlintSurfaceRole::TextEditor) {
            const DesktopWindow *editor = g_desktop.textEditorWindow();
            const uint32_t width = editor && editor->bounds.width > 0 ? static_cast<uint32_t>(editor->bounds.width) : 700u;
            const uint32_t height = editor && editor->bounds.height > 0 ? static_cast<uint32_t>(editor->bounds.height) : 460u;
            auto window = std::make_unique<RadSlintWindowAdapter>(SlintSurfaceRole::TextEditor, g_text_editor_surface_id, g_text_editor_pixels, width, height, g_surface_stride);
            text_editor_window_ = window.get();
            next_role_ = SlintSurfaceRole::Desktop;
            return window;
        }
        auto window = std::make_unique<RadSlintWindowAdapter>(SlintSurfaceRole::Desktop, g_desktop_surface_id, g_desktop_pixels, g_desktop_surface_width, g_desktop_surface_height, g_surface_stride);
        desktop_window_ = window.get();
        return window;
    }

    std::chrono::milliseconds duration_since_start() override {
        return std::chrono::milliseconds(rad_time_millis() - started_ms_);
    }

    void run_event_loop() override {
        for (;;) {
            tick();
            if (x86_ui_idle_frame) x86_ui_idle_frame();
            else rad_task_sleep_ms(16);
        }
    }

    void run_in_event_loop(Task task) override {
        std::move(task).run();
    }

    rad_status_t tick() {
        if (x86_ps2_poll_devices) x86_ps2_poll_devices();
        if (x86_virtio_input_poll) x86_virtio_input_poll();
        poll_input_device(keyboard_);
        poll_input_device(pointer_);
        apply_pending_terminal_resize();
        apply_pending_explorer_resize();
        apply_pending_editor_resize();
        drain_terminal_pty();
        // Reap client (userland) surfaces whose owning process has exited. Cheap
        // but not free (walks the process list), so throttle to a few times a
        // second rather than every frame.
        if ((++ipc_reap_ticks_ & 0x1f) == 0) {
            if (ipc_reap_dead_owners() > 0) {
                if (focused_ipc_surface_ && !ipc_is_surface(focused_ipc_surface_)) {
                    focused_ipc_surface_ = 0;
                }
            }
        }
        bool rendered = false;
        bool any_rendered = false;
        if (desktop_window_) {
            const rad_status_t status = desktop_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (desktop_window_->window().has_active_animations()) desktop_window_->request_redraw();
        }
        if (terminal_window_) {
            const rad_status_t status = terminal_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (terminal_window_->window().has_active_animations()) terminal_window_->request_redraw();
        }
        if (file_explorer_window_) {
            const rad_status_t status = file_explorer_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (file_explorer_window_->window().has_active_animations()) file_explorer_window_->request_redraw();
        }
        if (text_editor_window_) {
            const rad_status_t status = text_editor_window_->render_if_needed(&rendered);
            if (status != RAD_STATUS_OK) return status;
            any_rendered = any_rendered || rendered;
            if (text_editor_window_->window().has_active_animations()) text_editor_window_->request_redraw();
        }
        rad_compositor_set_framebuffers(&g_compositor, g_present_front, g_surface_stride, g_present_back, g_surface_stride);
        queue_cursor_damage();
        if (rad_compositor_compose_frame(&g_compositor) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
        if (g_compositor.last_present_rect_count > 0 || g_cursor_dirty) {
            draw_cursor(g_present_back, g_surface_stride);
        }
        if (g_compositor.last_present_rect_count == 0) {
            marker_once(&g_compositor_empty_marker_sent, "RAD_COMPOSITOR_EMPTY_FRAME_SKIP_OK");
        } else {
            marker_once(&g_compositor_copy_forward_marker_sent, "RAD_COMPOSITOR_COPY_FORWARD_OK");
            marker_once(&g_compositor_blit_marker_sent, "RAD_COMPOSITOR_BLIT_OK");
            for (uint32_t i = 0; i < g_compositor.last_present_rect_count; ++i) {
                const rad_compositor_rect_t& dirty = g_compositor.last_present_rects[i];
                rad_framebuffer_present_t present{};
                present.size = sizeof(present);
                present.pixels = g_present_back;
                present.stride_bytes = g_surface_stride * sizeof(uint32_t);
                present.rect.x = static_cast<uint32_t>(dirty.x);
                present.rect.y = static_cast<uint32_t>(dirty.y);
                present.rect.width = static_cast<uint32_t>(dirty.width);
                present.rect.height = static_cast<uint32_t>(dirty.height);
                const rad_status_t status = rad_framebuffer_present(framebuffer_, &present);
                if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_SUPPORTED) return status;
            }
            marker_once(&g_framebuffer_dirty_present_marker_sent, "RAD_FRAMEBUFFER_DIRTY_PRESENT_OK");
            uint32_t *tmp = g_present_front;
            g_present_front = g_present_back;
            g_present_back = tmp;
        }
        if (any_rendered && !g_boot_marker_sent) {
            marker_once(&g_boot_marker_sent, "RAD_SLINT_BOOT_SHELL_OK");
        }
        // The window tree is now laid out; it is safe to dispatch input into Slint.
        if (any_rendered && !ready_for_input_) {
            ready_for_input_ = true;
            marker_once(&g_input_ready_marker_sent, "RAD_SLINT_INPUT_READY_OK");
        }
        return RAD_STATUS_OK;
    }

    void request_terminal_redraw() {
        if (terminal_window_) terminal_window_->request_redraw();
    }

    void request_explorer_redraw() {
        if (file_explorer_window_) file_explorer_window_->request_redraw();
    }

    void request_editor_redraw() {
        if (text_editor_window_) text_editor_window_->request_redraw();
    }

    void focus_editor_document() {
        if (g_text_editor_shell) (*g_text_editor_shell)->invoke_focus_document();
    }

    void sync_terminal_bounds() {
        if (!terminal_window_) return;
        const DesktopWindow *terminal = g_desktop.terminalWindow();
        if (!terminal) return;
        rad_compositor_rect_t bounds{};
        bounds.x = terminal->bounds.x;
        bounds.y = terminal->bounds.y;
        bounds.width = terminal->bounds.width;
        bounds.height = terminal->bounds.height;
        const bool bounds_changed = !terminal_bounds_valid_
            || terminal_bounds_.x != bounds.x
            || terminal_bounds_.y != bounds.y
            || terminal_bounds_.width != bounds.width
            || terminal_bounds_.height != bounds.height;
        if (!bounds_changed) return;
        const bool size_changed = !terminal_bounds_valid_
            || terminal_bounds_.width != bounds.width
            || terminal_bounds_.height != bounds.height;
        rad_compositor_set_surface_bounds(&g_compositor, g_terminal_surface_id, &bounds);
        terminal_bounds_ = bounds;
        terminal_bounds_valid_ = 1;
        if (size_changed) {
            update_terminal_visible_text();
            if (g_terminal_shell) (*g_terminal_shell)->set_terminal(shared_string(g_terminal_visible_text));
            pending_terminal_resize_ = true;
            pending_terminal_width_ = static_cast<uint32_t>(bounds.width);
            pending_terminal_height_ = static_cast<uint32_t>(bounds.height);
        }
    }

    void sync_explorer_bounds() {
        if (!file_explorer_window_) return;
        const DesktopWindow *explorer = g_desktop.fileExplorerWindow();
        if (!explorer) return;
        rad_compositor_rect_t bounds{};
        bounds.x = explorer->bounds.x;
        bounds.y = explorer->bounds.y;
        bounds.width = explorer->bounds.width;
        bounds.height = explorer->bounds.height;
        const bool bounds_changed = !explorer_bounds_valid_
            || explorer_bounds_.x != bounds.x
            || explorer_bounds_.y != bounds.y
            || explorer_bounds_.width != bounds.width
            || explorer_bounds_.height != bounds.height;
        if (!bounds_changed) return;
        const bool size_changed = !explorer_bounds_valid_
            || explorer_bounds_.width != bounds.width
            || explorer_bounds_.height != bounds.height;
        rad_compositor_set_surface_bounds(&g_compositor, g_file_explorer_surface_id, &bounds);
        explorer_bounds_ = bounds;
        explorer_bounds_valid_ = 1;
        if (size_changed) {
            pending_explorer_resize_ = true;
            pending_explorer_width_ = static_cast<uint32_t>(bounds.width);
            pending_explorer_height_ = static_cast<uint32_t>(bounds.height);
        }
    }

    void sync_editor_bounds() {
        if (!text_editor_window_) return;
        const DesktopWindow *editor = g_desktop.textEditorWindow();
        if (!editor) return;
        rad_compositor_rect_t bounds{};
        bounds.x = editor->bounds.x;
        bounds.y = editor->bounds.y;
        bounds.width = editor->bounds.width;
        bounds.height = editor->bounds.height;
        const bool bounds_changed = !editor_bounds_valid_
            || editor_bounds_.x != bounds.x
            || editor_bounds_.y != bounds.y
            || editor_bounds_.width != bounds.width
            || editor_bounds_.height != bounds.height;
        if (!bounds_changed) return;
        const bool size_changed = !editor_bounds_valid_
            || editor_bounds_.width != bounds.width
            || editor_bounds_.height != bounds.height;
        rad_compositor_set_surface_bounds(&g_compositor, g_text_editor_surface_id, &bounds);
        editor_bounds_ = bounds;
        editor_bounds_valid_ = 1;
        if (size_changed) {
            pending_editor_resize_ = true;
            pending_editor_width_ = static_cast<uint32_t>(bounds.width);
            pending_editor_height_ = static_cast<uint32_t>(bounds.height);
        }
    }

private:
    void apply_pending_terminal_resize() {
        if (!pending_terminal_resize_ || !terminal_window_ || dispatching_input_) return;
        pending_terminal_resize_ = false;
        terminal_window_->update_size(pending_terminal_width_, pending_terminal_height_);
    }

    void apply_pending_explorer_resize() {
        if (!pending_explorer_resize_ || !file_explorer_window_ || dispatching_input_) return;
        pending_explorer_resize_ = false;
        file_explorer_window_->update_size(pending_explorer_width_, pending_explorer_height_);
    }

    void apply_pending_editor_resize() {
        if (!pending_editor_resize_ || !text_editor_window_ || dispatching_input_) return;
        pending_editor_resize_ = false;
        text_editor_window_->update_size(pending_editor_width_, pending_editor_height_);
    }

    void surface_origin(uint32_t surface_id, int32_t *x, int32_t *y) {
        *x = 0;
        *y = 0;
        if (surface_id == g_terminal_surface_id) {
            if (const DesktopWindow *w = g_desktop.terminalWindow()) {
                *x = w->bounds.x;
                *y = w->bounds.y;
            }
        } else if (surface_id == g_file_explorer_surface_id) {
            if (const DesktopWindow *w = g_desktop.fileExplorerWindow()) {
                *x = w->bounds.x;
                *y = w->bounds.y;
            }
        } else if (surface_id == g_text_editor_surface_id) {
            if (const DesktopWindow *w = g_desktop.textEditorWindow()) {
                *x = w->bounds.x;
                *y = w->bounds.y;
            }
        }
    }

    void dispatch_polled_event(const rad_input_event_t& event) {
        if (event.type == RAD_INPUT_EVENT_KEY) {
            // A focused client (userland) surface takes keyboard input first: queue it for
            // the owning process to poll, instead of dispatching into an in-kernel adapter.
            if (focused_ipc_surface_) {
                if (IpcSurfaceRecord *record = ipc_find(focused_ipc_surface_)) {
                    ipc_push_input(record, event);
                    return;
                }
                focused_ipc_surface_ = 0;
            }
            // Route keys to the FOCUSED window's adapter so the editor's TextEdit (and any
            // other Slint-driven window) receives them. Only the terminal adapter forwards
            // keys to the PTY (gated in dispatch_key_event). Fall back to the terminal
            // adapter when nothing is focused, preserving the prior behavior.
            RadSlintWindowAdapter *target = nullptr;
            switch (g_desktop.focusedWindowId()) {
            case TerminalWindowId: target = terminal_window_; break;
            case FileExplorerWindowId: target = file_explorer_window_; break;
            case TextEditorWindowId: target = text_editor_window_; break;
            default: break;
            }
            if (!target) target = terminal_window_ ? terminal_window_ : desktop_window_;
            if (!target) return;
            dispatching_input_ = true;
            target->dispatch_input_event(event);
            dispatching_input_ = false;
            apply_pending_terminal_resize();
            apply_pending_explorer_resize();
            apply_pending_editor_resize();
            return;
        }
        const bool is_press = event.type == RAD_INPUT_EVENT_POINTER_BUTTON && event.pressed;
        const bool is_release = event.type == RAD_INPUT_EVENT_POINTER_BUTTON && !event.pressed;

        RadSlintWindowAdapter *target = nullptr;
        int32_t local_x = 0;
        int32_t local_y = 0;

        if (pointer_grab_active_ && !is_press) {
            // A button is held: deliver motion/release to the grabbed surface even when the
            // cursor has moved off it. Without this a fast drag that outruns the window
            // stops (the motion hit-tests onto the desktop and the drag gesture dies).
            if (IpcSurfaceRecord *record = ipc_find(pointer_grab_surface_)) {
                rad_compositor_rect_t bounds{};
                compositor_surface_bounds(pointer_grab_surface_, &bounds);
                rad_input_event_t local = event;
                local.x = g_cursor_x - bounds.x;
                local.y = g_cursor_y - bounds.y;
                ipc_push_input(record, local);
                if (is_release) { pointer_grab_active_ = false; }
                return;
            }
            target = adapter_for_surface(pointer_grab_surface_);
            int32_t ox = 0, oy = 0;
            surface_origin(pointer_grab_surface_, &ox, &oy);
            local_x = g_cursor_x - ox;
            local_y = g_cursor_y - oy;
        } else {
            rad_compositor_input_result_t result{};
            if (rad_compositor_dispatch_input(&g_compositor, &event, &result) != RAD_STATUS_OK || !result.hit) {
                if (is_release) { g_desktop.endPointerGesture(); pointer_grab_active_ = false; }
                return;
            }
            // A hit on a client (userland) surface routes to its owning process.
            if (IpcSurfaceRecord *record = ipc_find(result.surface_id)) {
                if (is_press) {
                    pointer_grab_active_ = true;
                    pointer_grab_surface_ = result.surface_id;
                    rad_compositor_focus_surface(&g_compositor, result.surface_id);
                    focused_ipc_surface_ = result.surface_id;   // client takes keyboard focus
                    set_shell_state();
                    marker_once(&g_compositor_hit_marker_sent, "RAD_COMPOSITOR_HIT_TEST_OK");
                    marker_once(&g_compositor_input_marker_sent, "RAD_COMPOSITOR_INPUT_TRANSLATE_OK");
                }
                rad_input_event_t local = event;
                local.x = result.local_x;
                local.y = result.local_y;
                ipc_push_input(record, local);
                if (is_release) { pointer_grab_active_ = false; }
                return;
            }
            target = adapter_for_surface(result.surface_id);
            if (!target) {
                if (is_release) { g_desktop.endPointerGesture(); pointer_grab_active_ = false; }
                return;
            }
            local_x = result.local_x;
            local_y = result.local_y;
            marker_once(&g_compositor_hit_marker_sent, "RAD_COMPOSITOR_HIT_TEST_OK");
            marker_once(&g_compositor_input_marker_sent, "RAD_COMPOSITOR_INPUT_TRANSLATE_OK");
            if (is_press) {
                pointer_grab_active_ = true;
                pointer_grab_surface_ = result.surface_id;
                // Only RAISE window surfaces on click. The desktop surface holds the
                // wallpaper + topbar + applications menu and is full-screen opaque;
                // raising it above the terminal window (as clicking the menu button did)
                // hid the window entirely. The desktop stays at the bottom.
                uint32_t focus_window_id = 0;
                if (result.surface_id == g_terminal_surface_id) focus_window_id = TerminalWindowId;
                else if (result.surface_id == g_file_explorer_surface_id) focus_window_id = FileExplorerWindowId;
                else if (result.surface_id == g_text_editor_surface_id) focus_window_id = TextEditorWindowId;
                if (focus_window_id != 0) {
                    rad_compositor_focus_surface(&g_compositor, result.surface_id);
                    g_desktop.focusWindow(focus_window_id);
                    focused_ipc_surface_ = 0;   // WM window reclaims keyboard focus
                    set_shell_state();
                }
            }
        }

        if (!target) return;
        if (is_release) { g_desktop.endPointerGesture(); pointer_grab_active_ = false; }
        dispatching_input_ = true;
        target->dispatch_input_event(event, local_x, local_y);
        dispatching_input_ = false;
        apply_pending_terminal_resize();
        apply_pending_explorer_resize();
        apply_pending_editor_resize();
    }

    void poll_input_device(rad_device_t device) {
        if (!device) return;
        rad_input_event_t event{};
        rad_input_event_t pending_motion{};
        int has_pending_motion = 0;
        while (rad_input_read_event(device, &event) == RAD_STATUS_OK) {
            update_cursor_position(event);
            // Until the shell has completed its first render, the Slint window item tree
            // is not laid out yet. Dispatching an input event now drives try_dispatch_event
            // -> set_window_item_geometry against an unbuilt property/dependency graph and
            // corrupts Slint's reference-counted internals (intermittent #GP later). Drain
            // (and keep cursor tracking current) but do not dispatch until we are ready --
            // this also flushes any spurious PS/2 boot-time input.
            if (!ready_for_input_) continue;
            if (event.type == RAD_INPUT_EVENT_POINTER_MOTION) {
                pending_motion = event;
                has_pending_motion = 1;
                continue;
            }
            if (has_pending_motion) {
                dispatch_polled_event(pending_motion);
                has_pending_motion = 0;
            }
            dispatch_polled_event(event);
        }
        if (has_pending_motion) {
            dispatch_polled_event(pending_motion);
        }
    }

    RadSlintWindowAdapter *adapter_for_surface(uint32_t surface_id) {
        if (desktop_window_ && desktop_window_->surface_id() == surface_id) return desktop_window_;
        if (terminal_window_ && terminal_window_->surface_id() == surface_id) return terminal_window_;
        if (file_explorer_window_ && file_explorer_window_->surface_id() == surface_id) return file_explorer_window_;
        if (text_editor_window_ && text_editor_window_->surface_id() == surface_id) return text_editor_window_;
        return nullptr;
    }

    rad_framebuffer_t framebuffer_ = nullptr;
    rad_framebuffer_info_t info_{};
    rad_device_t keyboard_ = nullptr;
    rad_device_t pointer_ = nullptr;
    uint64_t started_ms_ = 0;
    SlintSurfaceRole next_role_ = SlintSurfaceRole::Desktop;
    RadSlintWindowAdapter *desktop_window_ = nullptr;
    RadSlintWindowAdapter *terminal_window_ = nullptr;
    RadSlintWindowAdapter *file_explorer_window_ = nullptr;
    RadSlintWindowAdapter *text_editor_window_ = nullptr;
    rad_compositor_rect_t terminal_bounds_{};
    int terminal_bounds_valid_ = 0;
    rad_compositor_rect_t explorer_bounds_{};
    int explorer_bounds_valid_ = 0;
    rad_compositor_rect_t editor_bounds_{};
    int editor_bounds_valid_ = 0;
    bool dispatching_input_ = false;
    bool ready_for_input_ = false;   // set true after the first render lays out the tree
    uint32_t ipc_reap_ticks_ = 0;      // throttle counter for client-surface reaping
    uint32_t focused_ipc_surface_ = 0; // client surface that currently has keyboard focus
    bool pointer_grab_active_ = false;   // a button is held: route motion/release to this
    uint32_t pointer_grab_surface_ = 0;  // surface even when the cursor leaves its bounds
    bool pending_terminal_resize_ = false;
    uint32_t pending_terminal_width_ = 0;
    uint32_t pending_terminal_height_ = 0;
    bool pending_explorer_resize_ = false;
    uint32_t pending_explorer_width_ = 0;
    uint32_t pending_explorer_height_ = 0;
    bool pending_editor_resize_ = false;
    uint32_t pending_editor_width_ = 0;
    uint32_t pending_editor_height_ = 0;
};

RadSlintPlatform *g_platform = nullptr;

void request_terminal_surface_redraw(void) {
    if (g_platform) g_platform->request_terminal_redraw();
}

const char *shell_status_text() {
    switch (g_desktop.terminalState()) {
    case TerminalAppState::Launching:
        return "framebuffer=primary shell=radlib state=terminal launching";
    case TerminalAppState::Running:
        return "framebuffer=primary shell=radlib state=terminal";
    case TerminalAppState::Closed:
    default:
        return g_desktop.applicationsMenuOpen()
            ? "framebuffer=primary shell=radlib state=applications menu"
            : "framebuffer=primary shell=radlib state=desktop";
    }
}

// Rebuild the dock's open-app icon model (in open order). Only called when the open set
// changes (launch/close) -- not every frame -- so the dock Repeater is not re-created and
// re-rendered on every set_shell_state.
void update_dock_icons() {
    if (!g_desktop_shell) return;
    uint32_t order[3];
    const int n = g_desktop.dockOrder(order);
    std::vector<RadDockIcon> icons;
    for (int i = 0; i < n; ++i) {
        RadDockIcon icon{};
        icon.app_id = static_cast<int>(order[i]);
        icon.kind = order[i] == TerminalWindowId ? 1
                  : order[i] == TextEditorWindowId ? 3 : 2;
        icons.push_back(icon);
    }
    (*g_desktop_shell)->set_dock_icons(std::make_shared<slint::VectorModel<RadDockIcon>>(std::move(icons)));
}

void set_shell_state(const char *status) {
    if (!g_desktop_shell || !g_terminal_shell || !g_file_explorer_shell || !g_text_editor_shell) return;
    const DesktopWindow *terminal = g_desktop.terminalWindow();
    const DesktopWindow *explorer = g_desktop.fileExplorerWindow();
    const DesktopWindow *editor = g_desktop.textEditorWindow();
    (*g_desktop_shell)->set_surface_width(static_cast<float>(g_desktop_surface_width));
    (*g_desktop_shell)->set_surface_height(static_cast<float>(g_desktop_surface_height));
    (*g_desktop_shell)->set_backend(shared_string("x86_64_grub / RADPx-OS"));
    (*g_desktop_shell)->set_status(shared_string(status ? status : shell_status_text()));
    (*g_desktop_shell)->set_applications_open(g_desktop.applicationsMenuOpen());
    (*g_desktop_shell)->set_terminal_open(g_desktop.terminalOpen());
    (*g_desktop_shell)->set_file_explorer_open(g_desktop.fileExplorerOpen());
    (*g_desktop_shell)->set_text_editor_open(g_desktop.textEditorOpen());
    (*g_desktop_shell)->set_dock_menu_app(g_desktop.dockMenuApp());
    {
        // Anchor the single Close dropdown to the Y of whichever open-app icon is selected.
        uint32_t order[3];
        const int n = g_desktop.dockOrder(order);
        const int menu_app = g_desktop.dockMenuApp();
        float menu_y = 132.0f;
        for (int i = 0; i < n; ++i) {
            if (static_cast<int>(order[i]) == menu_app) menu_y = 94.0f + static_cast<float>(i) * 70.0f + 15.0f;
        }
        (*g_desktop_shell)->set_dock_menu_y(menu_y);
    }
    (*g_terminal_shell)->set_terminal(shared_string(g_terminal_visible_text));
    (*g_terminal_shell)->set_terminal_loading(g_desktop.terminalLaunching());
    if (terminal) {
        (*g_terminal_shell)->set_terminal_window_title(shared_string(terminal->title));
        (*g_terminal_shell)->set_terminal_window_focused(terminal->focused);
        (*g_terminal_shell)->set_surface_width(static_cast<float>(terminal->bounds.width));
        (*g_terminal_shell)->set_surface_height(static_cast<float>(terminal->bounds.height));
        rad_compositor_set_surface_visible(&g_compositor, g_terminal_surface_id, terminal->state != DesktopWindowState::Closed);
        if (g_platform) g_platform->sync_terminal_bounds();
    }
    (*g_file_explorer_shell)->set_entries(shared_string(g_explorer_entries));
    (*g_file_explorer_shell)->set_explorer_loading(g_desktop.fileExplorerLaunching());
    if (explorer) {
        (*g_file_explorer_shell)->set_explorer_window_title(shared_string(explorer->title));
        (*g_file_explorer_shell)->set_explorer_window_focused(explorer->focused);
        (*g_file_explorer_shell)->set_surface_width(static_cast<float>(explorer->bounds.width));
        (*g_file_explorer_shell)->set_surface_height(static_cast<float>(explorer->bounds.height));
        rad_compositor_set_surface_visible(&g_compositor, g_file_explorer_surface_id, explorer->state != DesktopWindowState::Closed);
        if (g_platform) g_platform->sync_explorer_bounds();
    }
    (*g_text_editor_shell)->set_editor_loading(g_desktop.textEditorLaunching());
    (*g_text_editor_shell)->set_file_path(shared_string(g_editor_path));
    (*g_text_editor_shell)->set_document_text(shared_string(g_editor_document));
    if (editor) {
        (*g_text_editor_shell)->set_editor_window_title(shared_string(editor->title));
        (*g_text_editor_shell)->set_editor_window_focused(editor->focused);
        (*g_text_editor_shell)->set_surface_width(static_cast<float>(editor->bounds.width));
        (*g_text_editor_shell)->set_surface_height(static_cast<float>(editor->bounds.height));
        rad_compositor_set_surface_visible(&g_compositor, g_text_editor_surface_id, editor->state != DesktopWindowState::Closed);
        if (g_platform) g_platform->sync_editor_bounds();
    }
}

void render_ticks(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (g_platform) g_platform->tick();
    }
}

void launch_terminal(const char *terminal_text) {
    g_desktop.beginTerminalLaunch();
    update_dock_icons();
    set_shell_state();
    render_ticks(2);
    marker_once(&g_loading_marker_sent, "RAD_SLINT_TERMINAL_LOADING_OK");
    if (g_terminal_pty) {
        copy_terminal_text(terminal_text && *terminal_text ? terminal_text : g_terminal_text);
        g_desktop.terminalReady();
        set_shell_state();
        render_ticks(2);
        return;
    }
    copy_terminal_text("");

    char slave_name[64]{};
    rad_status_t status = rad_pty_open_pair("slint-terminal", &g_terminal_pty);
    if (status == RAD_STATUS_OK) {
        status = rad_pty_slave_name(g_terminal_pty, slave_name, sizeof(slave_name));
    }
    if (status == RAD_STATUS_OK && x86_user_spawn_process_with_stdio) {
        status = x86_user_spawn_process_with_stdio("/bin/radsh", rad_process_current_pid(), slave_name, &g_terminal_pid, &g_terminal_task);
        if (status == RAD_STATUS_OK) {
            rad_debug_marker("RAD_SLINT_APP_LAUNCH_PROCESS_OK");
            rad_debug_marker("RAD_SLINT_APP_LIVE_PTY_OK");
            // No placeholder banner: radsh prints its own greeting + user@host prompt.
        }
    } else if (rad_shell_launch_terminal_process) {
        status = rad_shell_launch_terminal_process();
        if (status != RAD_STATUS_OK) {
            rad_debug_marker("RAD_SLINT_APP_LAUNCH_PROCESS_FAIL");
        }
    }
    if (status != RAD_STATUS_OK) {
        rad_debug_marker("RAD_SLINT_APP_LAUNCH_PROCESS_FAIL");
        if (g_terminal_pty) {
            rad_pty_close(g_terminal_pty);
            g_terminal_pty = nullptr;
        }
    }

    if (!g_terminal_text[0]) copy_terminal_text(terminal_text);
    g_desktop.terminalReady();
    set_shell_state();
    render_ticks(2);
    marker_once(&g_ready_marker_sent, "RAD_SLINT_TERMINAL_READY_OK");
    marker_once(&g_wm_marker_sent, "RAD_SLINT_WM_OK");
    marker_once(&g_terminal_window_marker_sent, "RAD_SLINT_APP_TERMINAL_WINDOW_OK");
}

bool explorer_is_dot_entry(const char *name) {
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

// Enumerate g_explorer_path with the in-kernel VFS and push it into the explorer window as a
// real Slint model (one RadFileEntry per row) so the Repeater renders a clickable, icon'd
// list. A synthetic ".." row leads the list whenever we are below root. The parallel
// g_explorer_rows array lets navigate() resolve a clicked index back to a name/kind.
void populate_explorer_listing() {
    std::vector<RadFileEntry> rows;
    g_explorer_row_count = 0;

    const bool at_root = g_explorer_path[0] == '/' && g_explorer_path[1] == '\0';
    auto add_row = [&](const char *name, bool is_dir) {
        if (g_explorer_row_count >= static_cast<int>(sizeof(g_explorer_rows) / sizeof(g_explorer_rows[0]))) return;
        ExplorerRow &rec = g_explorer_rows[g_explorer_row_count++];
        const size_t copy = strnlen(name, sizeof(rec.name) - 1u);
        memcpy(rec.name, name, copy);
        rec.name[copy] = '\0';
        rec.is_dir = is_dir;
        RadFileEntry row{};
        row.name = shared_string(rec.name);
        row.is_dir = is_dir;
        rows.push_back(std::move(row));
    };

    if (!at_root) add_row("..", true);

    rad_dir_t dir = nullptr;
    if (rad_vfs_opendir(g_explorer_path, &dir) == RAD_STATUS_OK && dir) {
        rad_vfs_dirent_t entry{};
        while (rad_vfs_readdir(dir, &entry) == RAD_STATUS_OK) {
            if (strnlen(entry.name, sizeof(entry.name)) == 0 || explorer_is_dot_entry(entry.name)) continue;
            add_row(entry.name, entry.stat.is_directory != 0);
        }
        rad_vfs_closedir(dir);
    }

    if (g_file_explorer_shell) {
        (*g_file_explorer_shell)->set_entries_model(
            std::make_shared<slint::VectorModel<RadFileEntry>>(std::move(rows)));
        (*g_file_explorer_shell)->set_current_path(shared_string(g_explorer_path));
    }
}

// Handle a click on explorer row `index`: descend into a directory (or ".." to the parent),
// re-list, and redraw. Files are inert for now.
void explorer_navigate(int index) {
    if (index < 0 || index >= g_explorer_row_count) return;
    const ExplorerRow &row = g_explorer_rows[index];
    if (!row.is_dir) return;

    if (strcmp(row.name, "..") == 0) {
        size_t last_slash = 0;
        const size_t len = strlen(g_explorer_path);
        for (size_t i = 0; i < len; ++i) if (g_explorer_path[i] == '/') last_slash = i;
        g_explorer_path[last_slash == 0 ? 1u : last_slash] = '\0';
    } else {
        size_t len = strlen(g_explorer_path);
        const size_t nlen = strlen(row.name);
        const bool root = (len == 1);
        if ((root ? 1u : len + 1u) + nlen + 1u > sizeof(g_explorer_path)) return;
        if (!root) g_explorer_path[len++] = '/';
        memcpy(g_explorer_path + len, row.name, nlen);
        g_explorer_path[len + nlen] = '\0';
    }
    populate_explorer_listing();
    if (g_platform) g_platform->request_explorer_redraw();
    set_shell_state();
}

void launch_file_explorer() {
    g_desktop.beginFileExplorerLaunch();
    set_shell_state();
    render_ticks(2);
    populate_explorer_listing();
    g_desktop.fileExplorerReady();
    update_dock_icons();
    set_shell_state();
    if (g_platform) g_platform->request_explorer_redraw();
    render_ticks(2);
    marker_once(&g_wm_marker_sent, "RAD_SLINT_WM_OK");
}

void close_file_explorer_model_only() {
    g_desktop.closeWindow(FileExplorerWindowId);
    g_explorer_entries[0] = '\0';
    update_dock_icons();
    set_shell_state();
}

void store_editor_path(const char *path) {
    if (!path) return;
    const size_t copy = strnlen(path, sizeof(g_editor_path) - 1u);
    memcpy(g_editor_path, path, copy);
    g_editor_path[copy] = '\0';
}

void store_editor_document(const char *text) {
    if (!text) { g_editor_document[0] = '\0'; return; }
    const size_t copy = strnlen(text, sizeof(g_editor_document) - 1u);
    memmove(g_editor_document, text, copy);
    g_editor_document[copy] = '\0';
}

void launch_text_editor() {
    g_desktop.beginTextEditorLaunch();
    set_shell_state();
    render_ticks(2);
    g_desktop.textEditorReady();
    update_dock_icons();
    set_shell_state();
    if (g_platform) {
        g_platform->request_editor_redraw();
        g_platform->focus_editor_document();
    }
    render_ticks(2);
    marker_once(&g_wm_marker_sent, "RAD_SLINT_WM_OK");
}

void close_text_editor_model_only() {
    g_desktop.closeWindow(TextEditorWindowId);
    update_dock_icons();
    set_shell_state();
}

// Read the file at the editor's current file-path into g_editor_document (capped at ~64 KB)
// via the kernel VFS, and push it into the TextEdit. Emits RAD_SLINT_EDITOR_OPEN_OK.
void editor_open_requested() {
    if (!g_text_editor_shell) return;
    const slint::SharedString path = (*g_text_editor_shell)->get_file_path();
    store_editor_path(path.data());

    rad_file_t file = nullptr;
    if (rad_vfs_open(g_editor_path, RAD_VFS_READ, &file) == RAD_STATUS_OK && file) {
        size_t total = 0;
        while (total < sizeof(g_editor_document) - 1u) {
            size_t got = 0;
            const rad_status_t status = rad_vfs_read(file, g_editor_document + total,
                sizeof(g_editor_document) - 1u - total, &got);
            if (status != RAD_STATUS_OK || got == 0) break;
            total += got;
        }
        g_editor_document[total] = '\0';
        rad_vfs_close(file);
    } else {
        // No such file (or unreadable): start with an empty buffer so the user can create it.
        g_editor_document[0] = '\0';
    }

    (*g_text_editor_shell)->set_document_text(shared_string(g_editor_document));
    if (g_platform) {
        g_platform->request_editor_redraw();
        g_platform->focus_editor_document();
    }
    set_shell_state();
    rad_debug_marker("RAD_SLINT_EDITOR_OPEN_OK");
}

// Write g_editor_document to `path` via the kernel VFS (create/truncate). Emits
// RAD_SLINT_EDITOR_SAVE_OK.
void editor_save_requested(slint::SharedString path) {
    store_editor_path(path.data());
    // Snapshot the freshest text straight from the TextEdit so a Save without an intervening
    // edit callback still persists what is on screen.
    if (g_text_editor_shell) {
        const slint::SharedString current = (*g_text_editor_shell)->get_document_text();
        store_editor_document(current.data());
    }

    rad_file_t file = nullptr;
    if (rad_vfs_open(g_editor_path, RAD_VFS_WRITE | RAD_VFS_CREATE | RAD_VFS_TRUNCATE, &file) == RAD_STATUS_OK && file) {
        const size_t len = strnlen(g_editor_document, sizeof(g_editor_document));
        size_t offset = 0;
        while (offset < len) {
            size_t wrote = 0;
            const rad_status_t status = rad_vfs_write(file, g_editor_document + offset, len - offset, &wrote);
            if (status != RAD_STATUS_OK || wrote == 0) break;
            offset += wrote;
        }
        rad_vfs_close(file);
        rad_debug_marker("RAD_SLINT_EDITOR_SAVE_OK");
    } else {
        rad_debug_marker("RAD_SLINT_EDITOR_SAVE_FAIL");
    }
    set_shell_state();
}

void editor_text_changed(slint::SharedString text) {
    store_editor_document(text.data());
}

void editor_path_changed(slint::SharedString path) {
    store_editor_path(path.data());
}

// Kill the terminal's shell process and drop its PTY, and clear the on-screen buffer, so
// the next open is a fresh, empty radsh session (not the old scrollback / a dead shell).
void teardown_terminal_process() {
    if (g_terminal_pid > 0) {
        rad_process_kill(g_terminal_pid, 9); // SIGKILL
        g_terminal_pid = 0;
    }
    g_terminal_task = nullptr;
    if (g_terminal_pty) {
        rad_pty_close(g_terminal_pty);
        g_terminal_pty = nullptr;
    }
    g_terminal_scroll_lines = 0;
    copy_terminal_text("");
}

void close_terminal_model_only() {
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        g_desktop.closeWindow(window->id);
        teardown_terminal_process();
        update_dock_icons();
        set_shell_state();
        marker_once(&g_terminal_close_marker_sent, "RAD_SLINT_TERMINAL_CLOSE_OK");
    }
}

void run_compositor_input_smoke() {
    const DesktopWindow *window = g_desktop.terminalWindow();
    if (!window || window->state == DesktopWindowState::Closed) return;
    rad_input_event_t event{};
    event.size = sizeof(event);
    event.type = RAD_INPUT_EVENT_POINTER_BUTTON;
    event.code = RAD_INPUT_POINTER_BUTTON_LEFT;
    event.pressed = 1;
    event.x = window->bounds.x + 12;
    event.y = window->bounds.y + 10;
    event.buttons = RAD_INPUT_POINTER_BUTTON_LEFT;
    rad_compositor_input_result_t result{};
    if (rad_compositor_dispatch_input(&g_compositor, &event, &result) == RAD_STATUS_OK
        && result.hit
        && result.surface_id == g_terminal_surface_id
        && result.local_x == 12
        && result.local_y == 10) {
        marker_once(&g_compositor_hit_marker_sent, "RAD_COMPOSITOR_HIT_TEST_OK");
        marker_once(&g_compositor_input_marker_sent, "RAD_COMPOSITOR_INPUT_TRANSLATE_OK");
    }
}

void run_shell_smoke_actions() {
    if (g_shell_smoke_actions_done) return;
    g_shell_smoke_actions_done = 1;
    g_desktop.toggleApplicationsMenu();
    set_shell_state();
    marker_once(&g_menu_open_marker_sent, "RAD_SLINT_MENU_OPEN_OK");
    if (g_desktop.handleEscape()) {
        set_shell_state();
        marker_once(&g_menu_escape_marker_sent, "RAD_SLINT_MENU_ESCAPE_OK");
    }
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        if (g_desktop.moveWindow(window->id, 16, 10)) {
            set_shell_state();
            marker_once(&g_window_move_marker_sent, "RAD_SLINT_WINDOW_MOVE_OK");
        }
        g_desktop.endPointerGesture();
    }
    if (const DesktopWindow *window = g_desktop.terminalWindow()) {
        if (g_desktop.resizeWindow(window->id, 28, 18)) {
            set_shell_state();
            marker_once(&g_window_resize_marker_sent, "RAD_SLINT_WINDOW_RESIZE_OK");
        }
        g_desktop.endPointerGesture();
    }
    scroll_terminal(1);
    close_terminal_model_only();
    launch_terminal(g_terminal_text);
    run_compositor_input_smoke();
    marker_once(&g_terminal_relaunch_marker_sent, "RAD_SLINT_TERMINAL_RELAUNCH_OK");
}

void run_compositor_alpha_smoke() {
    uint32_t target[4]{};
    uint32_t back[4] = {0xff0000ffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
    uint32_t blend[4] = {0x80ff0000u, 0x80ff0000u, 0x80ff0000u, 0x80ff0000u};
    rad_compositor_t compositor{};
    if (rad_compositor_init(&compositor, target, 2, 2, 2) != RAD_STATUS_OK) return;
    uint32_t back_id = 0;
    rad_compositor_surface_config_t back_config{};
    back_config.size = sizeof(back_config);
    back_config.width = 2;
    back_config.height = 2;
    back_config.pixels = back;
    back_config.stride_pixels = 2;
    rad_compositor_create_surface(&compositor, &back_config, &back_id);
    uint32_t blend_id = 0;
    rad_compositor_surface_config_t blend_config{};
    blend_config.size = sizeof(blend_config);
    blend_config.width = 2;
    blend_config.height = 2;
    blend_config.z = 1;
    blend_config.flags = RAD_COMPOSITOR_SURFACE_ALPHA;
    blend_config.pixels = blend;
    blend_config.stride_pixels = 2;
    if (rad_compositor_create_surface(&compositor, &blend_config, &blend_id) == RAD_STATUS_OK
        && rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && target[0] != back[0]
        && target[0] != blend[0]) {
        marker_once(&g_compositor_alpha_marker_sent, "RAD_COMPOSITOR_ALPHA_OK");
    }
}

} // namespace

extern "C" void rad_slint_shell_set_ram_budget(uint64_t ram_budget_bytes) {
    g_ram_budget_hint = ram_budget_bytes;
}

extern "C" rad_compositor_tier_t rad_slint_shell_tier(void) {
    return g_active_tier;
}

extern "C" rad_status_t rad_slint_shell_start(rad_framebuffer_t framebuffer, const char *terminal_text) {
    if (g_slint_started) return RAD_STATUS_OK;
    if (!framebuffer) {
        // No framebuffer at all: headless target. Report the tier and let boot
        // proceed without a window manager.
        emit_tier_marker(RAD_COMPOSITOR_TIER_HEADLESS);
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    rad_framebuffer_info_t info{};
    rad_status_t status = rad_framebuffer_get_info(framebuffer, &info);
    if (status != RAD_STATUS_OK) return status;
    if (!info.pixels || info.pixel_format != RAD_PIXEL_FORMAT_XRGB8888 || info.width == 0 || info.height == 0) {
        // Present but unusable framebuffer: also headless from the shell's view.
        emit_tier_marker(RAD_COMPOSITOR_TIER_HEADLESS);
        return RAD_STATUS_NOT_SUPPORTED;
    }

    // Resolve the capability tier now that we have a usable framebuffer. XRGB8888
    // is 32bpp; the RAM budget is a per-platform hint (0 = unknown/ample -> FULL).
    const rad_compositor_tier_t tier = rad_compositor_select_tier(
        1, info.width, info.height, 32u, g_ram_budget_hint);
    emit_tier_marker(tier);

    // Size the surfaces to the actual framebuffer, capped by the tier, and allocate
    // them from the kernel heap instead of reserving four max-resolution static
    // arrays. The row stride is the (capped) framebuffer width, shared by all four.
    uint32_t max_w = MaxSurfaceWidth;
    uint32_t max_h = MaxSurfaceHeight;
    tier_surface_caps(tier, &max_w, &max_h);
    g_desktop_surface_width = clamp_surface_extent(info.width, max_w);
    g_desktop_surface_height = clamp_surface_extent(info.height, max_h);
    if (!alloc_surface_buffers(g_desktop_surface_width, g_desktop_surface_height)) {
        rad_debug_marker("RAD_COMPOSITOR_SURFACE_ALLOC_FAIL");
        return RAD_STATUS_NO_MEMORY;
    }
    marker_once(&g_surface_alloc_marker_sent, "RAD_COMPOSITOR_SURFACE_ALLOC_OK");
    // Budget gate: the six buffers must fit the tier's byte ceiling.
    const size_t surface_total_bytes = 6u * g_surface_buffer_pixels * sizeof(uint32_t);
    const size_t budget_ceiling_bytes =
        (tier == RAD_COMPOSITOR_TIER_LEAN) ? (24u * 1024u * 1024u) : (64u * 1024u * 1024u);
    if (surface_total_bytes <= budget_ceiling_bytes) {
        marker_once(&g_surface_budget_marker_sent, "RAD_COMPOSITOR_BUDGET_OK");
    } else {
        rad_debug_marker("RAD_COMPOSITOR_BUDGET_EXCEEDED");
    }

    const size_t surface_bytes = g_surface_buffer_pixels * sizeof(uint32_t);
    memset(g_desktop_pixels, 0, surface_bytes);
    memset(g_terminal_pixels, 0, surface_bytes);
    memset(g_file_explorer_pixels, 0, surface_bytes);
    memset(g_text_editor_pixels, 0, surface_bytes);
    memset(g_present_front_pixels, 0x1f, surface_bytes);
    memset(g_present_back_pixels, 0x1f, surface_bytes);
    g_desktop.setDesktopExtent(g_desktop_surface_width, g_desktop_surface_height);
    g_cursor_x = static_cast<int32_t>(g_desktop_surface_width / 2u);
    g_cursor_y = static_cast<int32_t>(g_desktop_surface_height / 2u);
    g_drawn_cursor_x = g_cursor_x;
    g_drawn_cursor_y = g_cursor_y;
    g_cursor_dirty = 1;
    status = rad_compositor_init(&g_compositor, g_present_back, g_desktop_surface_width, g_desktop_surface_height, g_surface_stride);
    if (status != RAD_STATUS_OK) return status;
    rad_compositor_set_framebuffers(&g_compositor, g_present_front, g_surface_stride, g_present_back, g_surface_stride);
    register_compositor_device();
    rad_compositor_surface_config_t desktop_config{};
    desktop_config.size = sizeof(desktop_config);
    desktop_config.app_id = "rad.desktop";
    desktop_config.title = "RAD Desktop";
    desktop_config.width = static_cast<int32_t>(g_desktop_surface_width);
    desktop_config.height = static_cast<int32_t>(g_desktop_surface_height);
    desktop_config.z = 0;
    desktop_config.pixels = g_desktop_pixels;
    desktop_config.stride_pixels = g_surface_stride;
    status = rad_compositor_create_surface(&g_compositor, &desktop_config, &g_desktop_surface_id);
    if (status != RAD_STATUS_OK) return status;
    const DesktopWindow *terminal_window = g_desktop.terminalWindow();
    rad_compositor_surface_config_t terminal_config{};
    terminal_config.size = sizeof(terminal_config);
    terminal_config.app_id = "rad.terminal";
    terminal_config.title = "Terminal";
    terminal_config.x = terminal_window ? terminal_window->bounds.x : 134;
    terminal_config.y = terminal_window ? terminal_window->bounds.y : 110;
    terminal_config.width = terminal_window ? terminal_window->bounds.width : 640;
    terminal_config.height = terminal_window ? terminal_window->bounds.height : 380;
    terminal_config.z = 10;
    terminal_config.pixels = g_terminal_pixels;
    terminal_config.stride_pixels = g_surface_stride;
    status = rad_compositor_create_surface(&g_compositor, &terminal_config, &g_terminal_surface_id);
    if (status != RAD_STATUS_OK) return status;
    const DesktopWindow *explorer_window = g_desktop.fileExplorerWindow();
    rad_compositor_surface_config_t explorer_config{};
    explorer_config.size = sizeof(explorer_config);
    explorer_config.app_id = "rad.files";
    explorer_config.title = "Files";
    explorer_config.x = explorer_window ? explorer_window->bounds.x : 220;
    explorer_config.y = explorer_window ? explorer_window->bounds.y : 150;
    explorer_config.width = explorer_window ? explorer_window->bounds.width : 560;
    explorer_config.height = explorer_window ? explorer_window->bounds.height : 360;
    explorer_config.z = 20;
    explorer_config.pixels = g_file_explorer_pixels;
    explorer_config.stride_pixels = g_surface_stride;
    status = rad_compositor_create_surface(&g_compositor, &explorer_config, &g_file_explorer_surface_id);
    if (status != RAD_STATUS_OK) return status;
    // The explorer starts Closed; hide it until launch_file_explorer shows it.
    rad_compositor_set_surface_visible(&g_compositor, g_file_explorer_surface_id, false);
    const DesktopWindow *editor_window = g_desktop.textEditorWindow();
    rad_compositor_surface_config_t editor_config{};
    editor_config.size = sizeof(editor_config);
    editor_config.app_id = "rad.editor";
    editor_config.title = "Editor";
    editor_config.x = editor_window ? editor_window->bounds.x : 260;
    editor_config.y = editor_window ? editor_window->bounds.y : 130;
    editor_config.width = editor_window ? editor_window->bounds.width : 700;
    editor_config.height = editor_window ? editor_window->bounds.height : 460;
    editor_config.z = 30;
    editor_config.pixels = g_text_editor_pixels;
    editor_config.stride_pixels = g_surface_stride;
    status = rad_compositor_create_surface(&g_compositor, &editor_config, &g_text_editor_surface_id);
    if (status != RAD_STATUS_OK) return status;
    // The editor starts Closed; hide it until launch_text_editor shows it.
    rad_compositor_set_surface_visible(&g_compositor, g_text_editor_surface_id, false);
    rad_compositor_focus_surface(&g_compositor, g_terminal_surface_id);
    marker_once(&g_compositor_surface_marker_sent, "RAD_COMPOSITOR_SURFACE_CREATE_OK");
    marker_once(&g_compositor_z_marker_sent, "RAD_COMPOSITOR_Z_ORDER_OK");
    run_compositor_alpha_smoke();

    auto *platform = new RadSlintPlatform(framebuffer, info);
    g_platform = platform;
    slint::platform::set_platform(std::unique_ptr<slint::platform::Platform>(platform));
    platform->set_next_role(SlintSurfaceRole::Desktop);
    g_desktop_shell = new slint::ComponentHandle<RadDesktopSurface>(RadDesktopSurface::create());
    platform->set_next_role(SlintSurfaceRole::Terminal);
    g_terminal_shell = new slint::ComponentHandle<RadTerminalSurface>(RadTerminalSurface::create());
    platform->set_next_role(SlintSurfaceRole::FileExplorer);
    g_file_explorer_shell = new slint::ComponentHandle<RadFileExplorerSurface>(RadFileExplorerSurface::create());
    platform->set_next_role(SlintSurfaceRole::TextEditor);
    g_text_editor_shell = new slint::ComponentHandle<RadTextEditorSurface>(RadTextEditorSurface::create());
    (*g_desktop_shell)->on_toggle_applications([]() {
        if (!g_desktop_shell) return;
        g_desktop.toggleApplicationsMenu();
        set_shell_state();
        if (g_desktop.applicationsMenuOpen()) marker_once(&g_menu_open_marker_sent, "RAD_SLINT_MENU_OPEN_OK");
    });
    (*g_desktop_shell)->on_launch_terminal([]() {
        launch_terminal(g_terminal_text);
    });
    (*g_desktop_shell)->on_launch_file_explorer([]() {
        launch_file_explorer();
    });
    (*g_desktop_shell)->on_launch_text_editor([]() {
        launch_text_editor();
    });
    // Dock right-click "Close" dropdown: open the per-icon menu, dismiss it, and the
    // Close actions for each app.
    (*g_desktop_shell)->on_dock_context_menu([](int app) {
        g_desktop.toggleDockMenu(app);
        set_shell_state();
    });
    (*g_desktop_shell)->on_dock_dismiss([]() {
        g_desktop.closeDockMenu();
        set_shell_state();
    });
    (*g_desktop_shell)->on_focus_window_from_dock([](int app) {
        g_desktop.closeDockMenu();
        if (app == 1) {
            g_desktop.focusWindow(TerminalWindowId);
            rad_compositor_focus_surface(&g_compositor, g_terminal_surface_id);
        } else if (app == 2) {
            g_desktop.focusWindow(FileExplorerWindowId);
            rad_compositor_focus_surface(&g_compositor, g_file_explorer_surface_id);
        } else if (app == 3) {
            g_desktop.focusWindow(TextEditorWindowId);
            rad_compositor_focus_surface(&g_compositor, g_text_editor_surface_id);
            if (g_platform) g_platform->focus_editor_document();
        }
        set_shell_state();
    });
    (*g_desktop_shell)->on_close_app_from_dock([](int app) {
        g_desktop.closeDockMenu();
        if (app == static_cast<int>(TerminalWindowId)) close_terminal_model_only();
        else if (app == static_cast<int>(FileExplorerWindowId)) close_file_explorer_model_only();
        else if (app == static_cast<int>(TextEditorWindowId)) close_text_editor_model_only();
        set_shell_state();
    });
    (*g_desktop_shell)->on_escape_pressed([]() {
        if (!g_desktop_shell) return;
        const bool menu_was_open = g_desktop.applicationsMenuOpen();
        if (g_desktop.handleEscape()) {
            set_shell_state();
            if (menu_was_open) marker_once(&g_menu_escape_marker_sent, "RAD_SLINT_MENU_ESCAPE_OK");
            else marker_once(&g_terminal_close_marker_sent, "RAD_SLINT_TERMINAL_CLOSE_OK");
        }
    });
    (*g_terminal_shell)->on_escape_pressed([]() {
        const bool menu_was_open = g_desktop.applicationsMenuOpen();
        if (g_desktop.handleEscape()) {
            set_shell_state();
            if (menu_was_open) marker_once(&g_menu_escape_marker_sent, "RAD_SLINT_MENU_ESCAPE_OK");
            else marker_once(&g_terminal_close_marker_sent, "RAD_SLINT_TERMINAL_CLOSE_OK");
        }
    });
    (*g_terminal_shell)->on_focus_terminal_window([]() {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            g_desktop.focusWindow(window->id);
            rad_compositor_focus_surface(&g_compositor, g_terminal_surface_id);
            marker_once(&g_compositor_z_marker_sent, "RAD_COMPOSITOR_Z_ORDER_OK");
        }
        set_shell_state();
    });
    (*g_terminal_shell)->on_close_terminal_window([]() {
        close_terminal_model_only();
    });
    (*g_terminal_shell)->on_move_terminal_window([](float, float) {
        // Ignore the Slint TouchArea-local delta (it feeds back and jitters as the window
        // moves under the pointer); drive the move from the stable screen cursor instead.
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.moveWindow(window->id, g_cursor_x, g_cursor_y)) {
                marker_once(&g_window_move_marker_sent, "RAD_SLINT_WINDOW_MOVE_OK");
            }
        }
        set_shell_state();
    });
    (*g_terminal_shell)->on_resize_terminal_window([](float, float) {
        if (const DesktopWindow *window = g_desktop.terminalWindow()) {
            if (g_desktop.resizeWindow(window->id, g_cursor_x, g_cursor_y)) {
                marker_once(&g_window_resize_marker_sent, "RAD_SLINT_WINDOW_RESIZE_OK");
            }
        }
        set_shell_state();
    });
    (*g_file_explorer_shell)->on_escape_pressed([]() {
        if (g_desktop.handleEscape()) {
            set_shell_state();
            marker_once(&g_menu_escape_marker_sent, "RAD_SLINT_MENU_ESCAPE_OK");
        }
    });
    (*g_file_explorer_shell)->on_focus_explorer_window([]() {
        g_desktop.focusWindow(FileExplorerWindowId);
        rad_compositor_focus_surface(&g_compositor, g_file_explorer_surface_id);
        marker_once(&g_compositor_z_marker_sent, "RAD_COMPOSITOR_Z_ORDER_OK");
        set_shell_state();
    });
    (*g_file_explorer_shell)->on_close_explorer_window([]() {
        close_file_explorer_model_only();
    });
    (*g_file_explorer_shell)->on_move_explorer_window([](float, float) {
        if (g_desktop.moveWindow(FileExplorerWindowId, g_cursor_x, g_cursor_y)) {
            marker_once(&g_window_move_marker_sent, "RAD_SLINT_WINDOW_MOVE_OK");
        }
        set_shell_state();
    });
    (*g_file_explorer_shell)->on_resize_explorer_window([](float, float) {
        if (g_desktop.resizeWindow(FileExplorerWindowId, g_cursor_x, g_cursor_y)) {
            marker_once(&g_window_resize_marker_sent, "RAD_SLINT_WINDOW_RESIZE_OK");
        }
        set_shell_state();
    });
    (*g_file_explorer_shell)->on_navigate([](int index) {
        explorer_navigate(index);
    });
    (*g_text_editor_shell)->on_escape_pressed([]() {
        if (g_desktop.handleEscape()) {
            set_shell_state();
            marker_once(&g_menu_escape_marker_sent, "RAD_SLINT_MENU_ESCAPE_OK");
        }
    });
    (*g_text_editor_shell)->on_focus_editor_window([]() {
        g_desktop.focusWindow(TextEditorWindowId);
        rad_compositor_focus_surface(&g_compositor, g_text_editor_surface_id);
        marker_once(&g_compositor_z_marker_sent, "RAD_COMPOSITOR_Z_ORDER_OK");
        if (g_platform) g_platform->focus_editor_document();
        set_shell_state();
    });
    (*g_text_editor_shell)->on_close_editor_window([]() {
        close_text_editor_model_only();
    });
    (*g_text_editor_shell)->on_move_editor_window([](float, float) {
        if (g_desktop.moveWindow(TextEditorWindowId, g_cursor_x, g_cursor_y)) {
            marker_once(&g_window_move_marker_sent, "RAD_SLINT_WINDOW_MOVE_OK");
        }
        set_shell_state();
    });
    (*g_text_editor_shell)->on_resize_editor_window([](float, float) {
        if (g_desktop.resizeWindow(TextEditorWindowId, g_cursor_x, g_cursor_y)) {
            marker_once(&g_window_resize_marker_sent, "RAD_SLINT_WINDOW_RESIZE_OK");
        }
        set_shell_state();
    });
    (*g_text_editor_shell)->on_open_requested([]() {
        editor_open_requested();
    });
    (*g_text_editor_shell)->on_save_requested([](slint::SharedString path) {
        editor_save_requested(path);
    });
    (*g_text_editor_shell)->on_text_changed([](slint::SharedString text) {
        editor_text_changed(text);
    });
    (*g_text_editor_shell)->on_path_changed([](slint::SharedString path) {
        editor_path_changed(path);
    });
    set_shell_state("framebuffer=primary shell=radlib state=desktop");
    (*g_desktop_shell)->show();
    (*g_terminal_shell)->show();
    (*g_file_explorer_shell)->show();
    (*g_text_editor_shell)->show();
    g_slint_started = 1;
    (void)terminal_text;
    render_ticks(2);
    // The window manager is up as soon as the desktop is composited. The terminal is NOT
    // auto-launched -- the user opens it from the applications menu (on_launch_terminal),
    // which is when the terminal process/window and their markers come up.
    marker_once(&g_wm_marker_sent, "RAD_SLINT_WM_OK");
    // Launch the reference client/server compositor app as a real userland
    // process. It creates its window over shared memory via /dev/compositor0 and
    // is composited + input-routed like any client -- proving the architecture
    // end to end (RAD_COMPOSITOR_IPC_SURFACE_OK fires when its surface lands).
    if (x86_user_spawn_process) {
        int32_t demo_pid = 0;
        rad_task_t demo_task = nullptr;
        if (x86_user_spawn_process("/bin/radwc-demo", rad_process_current_pid(), &demo_pid, &demo_task) == RAD_STATUS_OK) {
            marker_once(&g_wc_demo_marker_sent, "RAD_WC_DEMO_LAUNCH_OK");
        }
    }
#if defined(RAD_SLINT_SHELL_SELFTEST)
    // Self-test scaffolding only: drives the shell through menu/move/resize/close/
    // relaunch to emit the RAD_SLINT_* markers. It is destabilising in a live boot
    // (extra re-renders, a terminal relaunch, an input smoke) and the smoke validates
    // those markers via the hosted rad_os_shell self-test instead, so it is off by
    // default. The live shell must render, spawn the terminal, and return promptly so
    // the kernel main loop can start driving tick() (input + pty drain + present).
    run_shell_smoke_actions();
#endif
    render_ticks(2);
    return RAD_STATUS_OK;
}

extern "C" void rad_slint_shell_set_terminal_text(const char *terminal_text) {
    if (!g_slint_started) return;
    // When the WM terminal owns a live radsh PTY, its text comes solely from draining that
    // PTY (drain_terminal_pty). The kernel main loop still calls this with the base-terminal
    // boot transcript whenever it is dirty; applying it here would overwrite radsh's output
    // with stale/empty transcript text every frame, making the terminal flicker to black.
    // radsh owns the buffer while it is running, so ignore the transcript feed.
    if (g_terminal_pty) return;
    copy_terminal_text(terminal_text);
    if (g_desktop.terminalOpen()) g_desktop.terminalReady();
    set_shell_state();
}

extern "C" void rad_slint_shell_poll(void) {
    if (!g_slint_started || !g_platform) return;
    g_platform->tick();
}

extern "C" int rad_slint_shell_ready(void) {
    // Ready once the DESKTOP is composited (WM up), not once the terminal is ready: the
    // terminal is no longer auto-launched, so gating on the terminal-ready marker would
    // keep the kernel main loop falling back to the base framebuffer terminal forever.
    return g_slint_started && g_wm_marker_sent;
}
