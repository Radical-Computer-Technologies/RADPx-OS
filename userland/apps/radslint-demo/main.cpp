/* radslint-demo - a userland application that renders a real Slint UI (the same
 * Slint software renderer the kernel uses, here linked into a userland process)
 * into a shared-memory surface composited by the kernel. It gets the WM's
 * server-side window frame (title bar + close button) for free, exactly like
 * any other compositor client.
 *
 * This is the pattern the in-kernel Terminal / File Explorer / Text Editor
 * windows will be ported onto: an app is a separate process = Slint over
 * libradcompositor. */
#include "app.h"

#include "radcompositor.h"

#include <slint-platform.h>

#include <chrono>
#include <memory>
#include <span>

using namespace slint::platform;

static const uint32_t CONTENT_W = 400;
static const uint32_t CONTENT_H = 260;

static rad_wc_surface_t g_surface;      // the shm-backed compositor surface
static uint32_t g_frame = 0;

extern "C" long rad_syscall6(long, long, long, long, long, long, long);
static void say(const char *msg) {
    long n = 0;
    while (msg[n]) ++n;
    rad_syscall6(1, 1, (long)msg, n, 0, 0, 0);
}

// A window adapter whose swap-chain is the client's shared-memory surface.
class ClientAdapter : public WindowAdapter {
public:
    ClientAdapter()
        : renderer_(SoftwareRenderer::RepaintBufferType::NewBuffer),
          size_({CONTENT_W, CONTENT_H}) {}

    AbstractRenderer &renderer() override { return renderer_; }
    slint::PhysicalSize size() override { return size_; }
    void request_redraw() override { needs_redraw_ = true; }

    // Render the Slint scene into the shm surface (XRGB8888) and commit the
    // touched region to the compositor. Returns true if anything was drawn.
    bool render_frame() {
        needs_redraw_ = false;
        uint32_t *pixels = g_surface.pixels;
        const uint32_t stride = g_surface.stride;
        if (!pixels) return false;
        // Render the whole scene into an Rgb8 scratch buffer, then convert to the
        // surface's XRGB8888 shared memory.
        renderer_.render(std::span<slint::Rgb8Pixel>(rgb_buffer_, CONTENT_W * CONTENT_H), CONTENT_W);
        for (uint32_t y = 0; y < CONTENT_H; ++y) {
            const slint::Rgb8Pixel *src = rgb_buffer_ + (y * CONTENT_W);
            uint32_t *dst = pixels + (y * stride);
            for (uint32_t x = 0; x < CONTENT_W; ++x) {
                dst[x] = 0xff000000u | (uint32_t(src[x].r) << 16) | (uint32_t(src[x].g) << 8) | uint32_t(src[x].b);
            }
        }
        rad_wc_surface_commit(&g_surface, 0, 0, (int)CONTENT_W, (int)CONTENT_H);
        return true;
    }

    SoftwareRenderer renderer_;
    slint::PhysicalSize size_;
    slint::Rgb8Pixel rgb_buffer_[CONTENT_W * CONTENT_H];
    bool needs_redraw_ = true;
};

class ClientPlatform : public Platform {
public:
    std::unique_ptr<WindowAdapter> create_window_adapter() override {
        auto adapter = std::make_unique<ClientAdapter>();
        adapter_ = adapter.get();
        return adapter;
    }
    // Slint animations need a monotonic clock; frame*16ms is close enough.
    std::chrono::milliseconds duration_since_start() override {
        return std::chrono::milliseconds((long long)g_frame * 16);
    }
    ClientAdapter *adapter_ = nullptr;
};

static ClientPlatform *g_platform = nullptr;

static slint::LogicalPosition pos_of(const rad_wc_input_event_t &e) {
    return slint::LogicalPosition({(float)e.x, (float)e.y});
}

int main(void) {
    say("radslint-demo: starting userland Slint client\n");
    if (rad_wc_surface_open(&g_surface, "radslint-demo", CONTENT_W, CONTENT_H, 560, 200, 27) != 0) {
        say("radslint-demo: FAILED to open surface\n");
        return 1;
    }

    auto platform = std::make_unique<ClientPlatform>();
    g_platform = platform.get();
    slint::platform::set_platform(std::move(platform));

    auto app = AppWindow::create();
    app->show();
    // Tell Slint the window size so it lays out and renders the scene (without a
    // resize event the component is never laid out and the surface stays blank).
    if (g_platform->adapter_) {
        auto &w = g_platform->adapter_->window();
        w.dispatch_scale_factor_change_event(1.0f);
        w.dispatch_resize_event(slint::LogicalSize({(float)CONTENT_W, (float)CONTENT_H}));
        w.dispatch_window_active_changed_event(true);
        g_platform->adapter_->request_redraw();
    }
    say("radslint-demo: RAD_SLINT_USERLAND_APP_OK window is up\n");

    int running = 1;
    while (running) {
        slint::platform::update_timers_and_animations();

        rad_wc_input_event_t ev;
        int r;
        auto &win = g_platform->adapter_->window();
        while ((r = rad_wc_surface_poll_input(&g_surface, &ev)) == 1) {
            if (ev.type == RAD_WC_EVENT_CLOSE) {
                say("radslint-demo: RAD_SLINT_USERLAND_CLOSE_OK\n");
                running = 0;
            } else if (ev.type == RAD_WC_EVENT_POINTER_MOTION) {
                win.dispatch_pointer_move_event(pos_of(ev));
            } else if (ev.type == RAD_WC_EVENT_POINTER_BUTTON) {
                if (ev.pressed) win.dispatch_pointer_press_event(pos_of(ev), slint::PointerEventButton::Left);
                else win.dispatch_pointer_release_event(pos_of(ev), slint::PointerEventButton::Left);
            }
        }
        if (r < 0) break;

        if (g_platform->adapter_) g_platform->adapter_->render_frame();
        ++g_frame;
        rad_wc_sleep_ms(16);
    }

    rad_wc_surface_close(&g_surface);
    return 0;
}
