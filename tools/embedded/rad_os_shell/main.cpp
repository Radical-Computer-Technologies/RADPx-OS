#include <RADUi/RADSlintFramebufferBackend.h>
#include <radboot.h>
#include <radkernel/radkernel.h>

#include "RADCompositorModel.h"
#include "RADCompositorCore.h"
#include "RADDesktop.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
uint32_t framebuffer[640u * 360u];

struct Capture {
    std::string text;
};

void capture_write(const char *text, void *context) {
    auto *capture = static_cast<Capture*>(context);
    if (capture && text) {
        capture->text += text;
    }
}

std::string run_command(const char *line) {
    Capture capture;
    rad_terminal_execute(line, capture_write, &capture);
    return capture.text;
}

class KernelPtyTranscript {
public:
    KernelPtyTranscript() {
        if (rad_pty_open_pair("slint-shell", &pty_) != RAD_STATUS_OK) return;
        char slaveName[64]{};
        if (rad_pty_slave_name(pty_, slaveName, sizeof(slaveName)) != RAD_STATUS_OK) return;
        if (rad_tty_open(slaveName, &slave_) != RAD_STATUS_OK) return;
        rad_tty_window_size_t size{24, 78};
        rad_pty_resize(pty_, &size);
        ready_ = true;
    }

    ~KernelPtyTranscript() {
        if (slave_) rad_tty_close(slave_);
        if (pty_) rad_pty_close(pty_);
    }

    bool ready() const {
        return ready_;
    }

    std::string run(const char *line) {
        if (!ready_ || !line) return {};
        append("$ ");
        append(line);
        append("\n");

        size_t written = 0;
        rad_pty_write_master(pty_, line, std::strlen(line), &written);
        rad_pty_write_master(pty_, "\n", 1, &written);
        rad_terminal_poll_tty(slave_);
        drain();
        return text_;
    }

private:
    void append(const char *text) {
        if (text) text_ += text;
    }

    void drain() {
        char buffer[512];
        size_t count = 0;
        while (rad_pty_read_master(pty_, buffer, sizeof(buffer), &count) == RAD_STATUS_OK && count > 0) {
            text_.append(buffer, count);
        }
    }

    rad_pty_t pty_ = nullptr;
    rad_tty_t slave_ = nullptr;
    bool ready_ = false;
    std::string text_;
};

slint::SharedString ss(const std::string& value) {
    return slint::SharedString(std::string_view(value));
}

rad_status_t register_framebuffer() {
    rad_framebuffer_config_t fb{};
    fb.size = sizeof(fb);
    fb.name = "/dev/fb0";
    fb.info.size = sizeof(fb.info);
    fb.info.width = 640u;
    fb.info.height = 360u;
    fb.info.stride_bytes = 640u * sizeof(uint32_t);
    fb.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    fb.info.pixels = framebuffer;
    fb.output_type = RAD_DISPLAY_OUTPUT_MEMORY;
    fb.connector = "host-memory";
    fb.mode_count = 1u;
    fb.preferred_mode = 0u;
    fb.modes[0].width = fb.info.width;
    fb.modes[0].height = fb.info.height;
    fb.modes[0].refresh_hz = 60u;
    fb.modes[0].stride_bytes = fb.info.stride_bytes;
    fb.modes[0].pixel_format = fb.info.pixel_format;
    fb.primary = 1;
    rad_framebuffer_ops_t ops{};
    return rad_framebuffer_register_ex(&fb, &ops);
}

struct CompositorSelfTestResult {
    bool surfaceCreate = false;
    bool offscreenRender = false;
    bool blit = false;
    bool hitTest = false;
    bool inputTranslate = false;
    bool zOrder = false;
    bool alpha = false;
    bool damageQueue = false;
    bool copyForward = false;
    bool exposedDamage = false;
    bool emptyFrame = false;
};

CompositorSelfTestResult run_compositor_self_test() {
    CompositorSelfTestResult result;
    uint32_t target[16u * 16u]{};
    uint32_t shadow[16u * 16u]{};
    uint32_t desktop[16u * 16u]{};
    uint32_t front[8u * 8u]{};
    uint32_t alpha[4u * 4u]{};
    for (uint32_t& pixel : desktop) pixel = 0xff0000ffu;
    for (uint32_t& pixel : shadow) pixel = 0xff07111fu;
    for (uint32_t& pixel : front) pixel = 0xffff0000u;
    for (uint32_t& pixel : alpha) pixel = 0x8000ff00u;

    rad_compositor_t compositor{};
    if (rad_compositor_init(&compositor, target, 16, 16, 16) != RAD_STATUS_OK) return result;
    rad_compositor_set_framebuffers(&compositor, shadow, 16, target, 16);

    uint32_t desktopId = 0;
    rad_compositor_surface_config_t desktopConfig{};
    desktopConfig.size = sizeof(desktopConfig);
    desktopConfig.app_id = "desktop";
    desktopConfig.title = "Desktop";
    desktopConfig.width = 16;
    desktopConfig.height = 16;
    desktopConfig.z = 0;
    desktopConfig.pixels = desktop;
    desktopConfig.stride_pixels = 16;
    result.surfaceCreate = rad_compositor_create_surface(&compositor, &desktopConfig, &desktopId) == RAD_STATUS_OK;

    uint32_t frontId = 0;
    rad_compositor_surface_config_t frontConfig{};
    frontConfig.size = sizeof(frontConfig);
    frontConfig.app_id = "terminal";
    frontConfig.title = "Terminal";
    frontConfig.x = 4;
    frontConfig.y = 4;
    frontConfig.width = 8;
    frontConfig.height = 8;
    frontConfig.z = 10;
    frontConfig.pixels = front;
    frontConfig.stride_pixels = 8;
    result.surfaceCreate = result.surfaceCreate && rad_compositor_create_surface(&compositor, &frontConfig, &frontId) == RAD_STATUS_OK;
    result.offscreenRender = front[0] == 0xffff0000u && desktop[0] == 0xff0000ffu;
    result.zOrder = rad_compositor_focus_surface(&compositor, frontId) == RAD_STATUS_OK;

    rad_compositor_input_result_t input{};
    result.hitTest = rad_compositor_hit_test(&compositor, 5, 5, &input) == RAD_STATUS_OK
        && input.surface_id == frontId
        && input.local_x == 1
        && input.local_y == 1;
    rad_input_event_t event{};
    event.size = sizeof(event);
    event.type = RAD_INPUT_EVENT_POINTER_BUTTON;
    event.x = 6;
    event.y = 7;
    result.inputTranslate = rad_compositor_dispatch_input(&compositor, &event, &input) == RAD_STATUS_OK
        && input.surface_id == frontId
        && input.local_x == 2
        && input.local_y == 3;

    result.blit = rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && target[0] == 0xff0000ffu
        && target[5u * 16u + 5u] == 0xffff0000u;
    result.copyForward = compositor.last_present_rect_count > 0;
    result.emptyFrame = rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && compositor.last_present_rect_count == 0;
    rad_compositor_rect_t dirty{1, 1, 2, 2};
    result.damageQueue = rad_compositor_queue_damage(&compositor, frontId, &dirty, 0) == RAD_STATUS_OK
        && rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && compositor.last_present_rect_count > 0;
    rad_compositor_rect_t exposed{4, 4, 8, 8};
    result.exposedDamage = rad_compositor_queue_damage(&compositor, frontId, &exposed, RAD_COMPOSITOR_DAMAGE_EXPOSED) == RAD_STATUS_OK
        && rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && compositor.last_present_rect_count > 0;

    uint32_t alphaId = 0;
    rad_compositor_surface_config_t alphaConfig{};
    alphaConfig.size = sizeof(alphaConfig);
    alphaConfig.app_id = "alpha";
    alphaConfig.title = "Alpha";
    alphaConfig.x = 0;
    alphaConfig.y = 0;
    alphaConfig.width = 4;
    alphaConfig.height = 4;
    alphaConfig.z = 20;
    alphaConfig.flags = RAD_COMPOSITOR_SURFACE_ALPHA;
    alphaConfig.pixels = alpha;
    alphaConfig.stride_pixels = 4;
    result.alpha = rad_compositor_create_surface(&compositor, &alphaConfig, &alphaId) == RAD_STATUS_OK
        && rad_compositor_compose_frame(&compositor) == RAD_STATUS_OK
        && target[0] != 0x8000ff00u
        && target[0] != 0xff0000ffu;
    return result;
}
}

int main(int argc, char **argv) {
    const bool self_test = argc > 1 && std::strcmp(argv[1], "--self-test") == 0;

    rad_boot_info_t boot{};
    radboot_prepare_default(&boot, "linux_sim", "rad-os-shell", "/dev/console", "none");
    rad_kernel_config_t config{};
    config.backend_name = "linux_sim";
    config.boot_info = &boot;
    if (rad_kernel_init(&config) != RAD_STATUS_OK) {
        std::cerr << "rad_kernel_init failed\n";
        return EXIT_FAILURE;
    }
    if (register_framebuffer() != RAD_STATUS_OK) {
        std::cerr << "framebuffer registration failed\n";
        return EXIT_FAILURE;
    }

    RADUi::Slint::FramebufferPlatformConfig ui_config;
    ui_config.maxFrames = self_test ? 1u : 0u;
    ui_config.keyboardInputDevice = "/dev/input/event0";
    ui_config.pointerInputDevice = "/dev/input/event1";
    std::shared_ptr<RADUi::Slint::SlintBackend> backend;
    const rad_status_t ui_status = RADUi::Slint::tryInstallFramebufferBackend(ui_config, &backend);
    if (ui_status != RAD_STATUS_OK) {
        std::cerr << "Slint framebuffer backend failed: " << ui_status << "\n";
        return EXIT_FAILURE;
    }

    std::string terminal;
    bool terminalLoadingObserved = false;
    bool terminalReadyObserved = false;
    bool windowManagerObserved = false;
    bool terminalWindowObserved = false;
    bool menuOpenObserved = false;
    bool menuEscapeObserved = false;
    bool windowMoveObserved = false;
    bool windowResizeObserved = false;
    bool terminalScrollObserved = false;
    bool terminalCloseObserved = false;
    bool terminalRelaunchObserved = false;
    bool secondWindowObserved = false;
    bool focusSwitchObserved = false;
    bool launcherObserved = false;
    bool taskbarObserved = false;
    bool windowCloseObserved = false;
    CompositorSelfTestResult compositorSelfTest{};
    if (self_test) compositorSelfTest = run_compositor_self_test();

    auto ui = RadDesktop::create();
    RADCompositor::Model desktop;
    // A second app so the launcher and taskbar are genuinely multi-app, not
    // Terminal-only. Idempotent with the self-test's own registration.
    desktop.registerApp("rad.sysinfo", "System");

    auto sysinfoBody = [&]() -> std::string {
        std::string s;
        s += "RAD OS \xE2\x80\x94 System\n\n";
        s += "backend : "; s += rad_kernel_backend_name(); s += "\n";
        s += "version : "; s += rad_kernel_version_string(); s += "\n";
        s += "shell   : radlib compositor\n";
        s += "display : primary framebuffer\n";
        s += "windows : multi-window desktop\n";
        return s;
    };
    auto bodyFor = [&](const RADCompositor::Window *w) -> std::string {
        if (!w) return {};
        if (std::strcmp(w->appId, "rad.terminal") == 0) return terminal;
        if (std::strcmp(w->appId, "rad.sysinfo") == 0) return sysinfoBody();
        return {};
    };
    auto refreshShell = [&]() {
        ui->set_backend(ss(std::string(rad_kernel_backend_name()) + " / " + rad_kernel_version_string()));
        ui->set_status(ss(std::string("framebuffer=primary shell=radlib state=") + desktop.statusText()));
        ui->set_clock(ss("online"));
        ui->set_applications_open(desktop.applicationsMenuOpen());

        std::vector<RadWinInfo> wins;
        for (size_t i = 0; i < desktop.windowCount(); ++i) {
            const RADCompositor::Window *w = desktop.window(i);
            if (!w || w->state == RADCompositor::WindowState::Closed) continue;
            RadWinInfo info;
            info.id = static_cast<int>(w->id);
            info.title = ss(w->title);
            info.body = ss(bodyFor(w));
            info.x = static_cast<float>(w->bounds.x);
            info.y = static_cast<float>(w->bounds.y);
            info.w = static_cast<float>(w->bounds.width);
            info.h = static_cast<float>(w->bounds.height);
            info.focused = w->focused;
            info.loading = w->state == RADCompositor::WindowState::Loading;
            wins.push_back(info);
        }
        ui->set_windows(std::make_shared<slint::VectorModel<RadWinInfo>>(std::move(wins)));

        std::vector<RadApp> apps;
        for (size_t i = 0; i < desktop.appCount(); ++i) {
            const RADCompositor::AppDescriptor *a = desktop.app(i);
            if (!a) continue;
            RadApp ra;
            ra.id = ss(a->id);
            ra.title = ss(a->title);
            apps.push_back(ra);
        }
        ui->set_apps(std::make_shared<slint::VectorModel<RadApp>>(std::move(apps)));

        if (desktop.terminalLaunching()) terminalLoadingObserved = true;
        if (desktop.terminalState() == RADCompositor::TerminalAppState::Running) terminalReadyObserved = true;
        if (desktop.windowCount() != 0 && desktop.appCount() != 0) windowManagerObserved = true;
        if (desktop.applicationsMenuOpen()) menuOpenObserved = true;
        const RADCompositor::Window *tw = desktop.terminalWindow();
        if (tw && tw->state != RADCompositor::WindowState::Closed) terminalWindowObserved = true;
    };
    auto launchTerminal = [&]() {
        desktop.beginTerminalLaunch();
        refreshShell();

        terminal.clear();
        KernelPtyTranscript ptyTerminal;
        if (ptyTerminal.ready()) {
            ptyTerminal.run("bootinfo");
            ptyTerminal.run("cores");
            ptyTerminal.run("mem");
            ptyTerminal.run("devices");
            ptyTerminal.run("perf");
            ptyTerminal.run("latency");
            terminal = ptyTerminal.run("fb");
        } else {
            terminal += "$ bootinfo\n" + run_command("bootinfo");
            terminal += "$ cores\n" + run_command("cores");
            terminal += "$ mem\n" + run_command("mem");
            terminal += "$ devices\n" + run_command("devices");
            terminal += "$ perf\n" + run_command("perf");
            terminal += "$ latency\n" + run_command("latency");
            terminal += "$ fb\n" + run_command("fb");
        }
        desktop.terminalReady();
        refreshShell();
    };
    auto launchAppById = [&](const std::string& appId) {
        if (appId == "rad.terminal") {
            launchTerminal();
        } else {
            const uint32_t id = desktop.launchApp(appId.c_str());
            if (id) desktop.setWindowState(id, RADCompositor::WindowState::Running);
            refreshShell();
        }
    };
    ui->on_toggle_applications([&]() {
        desktop.toggleApplicationsMenu();
        refreshShell();
    });
    ui->on_launch_app([&](slint::SharedString id) {
        launchAppById(std::string(id));
    });
    ui->on_escape_pressed([&]() {
        desktop.handleEscape();
        refreshShell();
    });
    ui->on_focus_window([&](int id) {
        desktop.focusWindow(static_cast<uint32_t>(id));
        refreshShell();
    });
    ui->on_close_window([&](int id) {
        desktop.closeWindow(static_cast<uint32_t>(id));
        refreshShell();
    });
    ui->on_move_window([&](int id, float dx, float dy) {
        desktop.moveWindow(static_cast<uint32_t>(id), static_cast<int32_t>(dx), static_cast<int32_t>(dy));
        refreshShell();
    });
    ui->on_resize_window([&](int id, float dx, float dy) {
        desktop.resizeWindow(static_cast<uint32_t>(id), static_cast<int32_t>(dx), static_cast<int32_t>(dy));
        refreshShell();
    });
    launchTerminal();
    if (self_test) {
        desktop.toggleApplicationsMenu();
        refreshShell();
        if (desktop.handleEscape()) {
            menuEscapeObserved = true;
            refreshShell();
        }
        if (const RADCompositor::Window *window = desktop.terminalWindow()) {
            const auto before = window->bounds;
            desktop.moveWindow(window->id, 12, 8);
            const RADCompositor::Window *moved = desktop.terminalWindow();
            windowMoveObserved = moved && moved->bounds.x == before.x + 12 && moved->bounds.y == before.y + 8;
            desktop.resizeWindow(window->id, 24, 16);
            const RADCompositor::Window *resized = desktop.terminalWindow();
            windowResizeObserved = resized && resized->bounds.width == before.width + 24 && resized->bounds.height == before.height + 16;
            terminalScrollObserved = !terminal.empty();
            refreshShell();
        }
        if (const RADCompositor::Window *window = desktop.terminalWindow()) {
            desktop.closeWindow(window->id);
            terminalCloseObserved = desktop.terminalState() == RADCompositor::TerminalAppState::Closed;
            refreshShell();
        }
        launchTerminal();
        terminalRelaunchObserved = desktop.terminalState() == RADCompositor::TerminalAppState::Running;

        // Multi-window: register + launch a second app and verify the freestanding
        // Model tracks multiple open windows with independent focus/z-order.
        desktop.registerApp("rad.sysinfo", "System");
        const uint32_t sysId = desktop.launchApp("rad.sysinfo");
        desktop.setWindowState(sysId, RADCompositor::WindowState::Running);
        refreshShell();
        secondWindowObserved = desktop.openWindowCount() >= 2;
        bool termFocused = false, sysFocused = false;
        if (const RADCompositor::Window *term = desktop.terminalWindow()) {
            desktop.focusWindow(term->id);
            const RADCompositor::Window *f = desktop.focusedWindow();
            termFocused = f && f->id == term->id && f->focused;
        }
        desktop.focusWindow(sysId);
        const RADCompositor::Window *f2 = desktop.focusedWindow();
        sysFocused = f2 && f2->id == sysId && f2->focused;
        focusSwitchObserved = secondWindowObserved && termFocused && sysFocused;
        refreshShell();

        // Launcher lists >=2 apps (Terminal + System); taskbar/dock lists both
        // open windows. Both are rendered by the RadDesktop Repeaters bound to
        // the apps / windows models refreshShell() just populated.
        launcherObserved = desktop.appCount() >= 2;
        taskbarObserved = desktop.openWindowCount() >= 2;

        // Generalized close on a non-terminal window: closing System drops the
        // open-window count and focus falls back to a surviving window.
        const size_t beforeClose = desktop.openWindowCount();
        desktop.closeWindow(sysId);
        windowCloseObserved = desktop.openWindowCount() == beforeClose - 1
            && desktop.focusedWindow() != nullptr;
        refreshShell();
    }
    ui->show();
    RADUi::run();

    uint32_t checksum = 0;
    for (uint32_t pixel : framebuffer) {
        checksum |= pixel;
    }
    if (self_test) {
        std::cout << "RAD OS Slint shell rendered checksum=0x" << std::hex << checksum << "\n";
        if (terminalLoadingObserved) std::cout << "RAD_SLINT_TERMINAL_LOADING_OK\n";
        if (terminalReadyObserved) std::cout << "RAD_SLINT_TERMINAL_READY_OK\n";
        if (windowManagerObserved) std::cout << "RAD_SLINT_WM_OK\n";
        if (terminalWindowObserved) std::cout << "RAD_SLINT_APP_TERMINAL_WINDOW_OK\n";
        if (menuOpenObserved) std::cout << "RAD_SLINT_MENU_OPEN_OK\n";
        if (menuEscapeObserved) std::cout << "RAD_SLINT_MENU_ESCAPE_OK\n";
        if (windowMoveObserved) std::cout << "RAD_SLINT_WINDOW_MOVE_OK\n";
        if (windowResizeObserved) std::cout << "RAD_SLINT_WINDOW_RESIZE_OK\n";
        if (terminalScrollObserved) std::cout << "RAD_SLINT_TERMINAL_SCROLL_OK\n";
        if (terminalCloseObserved) std::cout << "RAD_SLINT_TERMINAL_CLOSE_OK\n";
        if (terminalRelaunchObserved) std::cout << "RAD_SLINT_TERMINAL_RELAUNCH_OK\n";
        if (secondWindowObserved) std::cout << "RAD_SLINT_WINDOW_OPEN_OK\n";
        if (focusSwitchObserved) std::cout << "RAD_SLINT_FOCUS_SWITCH_OK\n";
        if (launcherObserved) std::cout << "RAD_SLINT_LAUNCHER_OK\n";
        if (taskbarObserved) std::cout << "RAD_SLINT_TASKBAR_OK\n";
        if (windowCloseObserved) std::cout << "RAD_SLINT_CLOSE_OK\n";
        if (compositorSelfTest.surfaceCreate) std::cout << "RAD_COMPOSITOR_SURFACE_CREATE_OK\n";
        if (compositorSelfTest.offscreenRender) std::cout << "RAD_COMPOSITOR_OFFSCREEN_RENDER_OK\n";
        if (compositorSelfTest.blit) std::cout << "RAD_COMPOSITOR_BLIT_OK\n";
        if (compositorSelfTest.hitTest) std::cout << "RAD_COMPOSITOR_HIT_TEST_OK\n";
        if (compositorSelfTest.inputTranslate) std::cout << "RAD_COMPOSITOR_INPUT_TRANSLATE_OK\n";
        if (compositorSelfTest.zOrder) std::cout << "RAD_COMPOSITOR_Z_ORDER_OK\n";
        if (compositorSelfTest.alpha) std::cout << "RAD_COMPOSITOR_ALPHA_OK\n";
        if (compositorSelfTest.damageQueue) std::cout << "RAD_COMPOSITOR_DAMAGE_QUEUE_OK\n";
        if (compositorSelfTest.copyForward) std::cout << "RAD_COMPOSITOR_COPY_FORWARD_OK\n";
        if (compositorSelfTest.exposedDamage) std::cout << "RAD_COMPOSITOR_EXPOSED_DAMAGE_OK\n";
        if (compositorSelfTest.emptyFrame) std::cout << "RAD_COMPOSITOR_EMPTY_FRAME_SKIP_OK\n";
    }
    rad_kernel_shutdown();
    return checksum ? EXIT_SUCCESS : EXIT_FAILURE;
}
