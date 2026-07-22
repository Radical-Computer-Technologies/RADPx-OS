/* radwc-demo - a userland RADPx-OS application that renders into a shared-memory
 * surface composited by the kernel, and reacts to input routed back from the
 * compositor. It is the reference client for the client/server compositor path:
 * a separate process (not kernel code) owning a real, movable, interactive
 * window.
 *
 *   - creates a window surface over shared memory (/dev/compositor0)
 *   - draws a title bar + live animated body into the shared buffer each frame
 *   - polls routed input: drag the title bar to move the window, any key cycles
 *     the body color, Escape closes the window (the kernel then reaps it)
 */
#include "radcompositor.h"

#include <rad/syscalls.h>

#define WIN_W 460u
#define WIN_H 300u
#define WIN_X 470
#define WIN_Y 160
#define WIN_Z 26
#define TITLE_H 28

static void put_str(const char *s) {
    unsigned long n = 0;
    while (s[n]) ++n;
    rad_syscall6(RAD_WC_SYS_WRITE, 1, (long)s, (long)n, 0, 0, 0);
}

static void fill_rect(uint32_t *px, uint32_t stride, uint32_t win_w, uint32_t win_h,
                      int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)win_w) w = (int)win_w - x;
    if (y + h > (int)win_h) h = (int)win_h - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        uint32_t *line = px + (uint32_t)(y + row) * stride + (uint32_t)x;
        for (int col = 0; col < w; ++col) line[col] = color;
    }
}

/* XRGB8888 opaque colors (high byte = 0xff = fully opaque). */
static const uint32_t g_body_colors[] = {
    0xff1e2a44u, 0xff203a2eu, 0xff3a2440u, 0xff402a1eu, 0xff243a44u,
};
#define N_BODY_COLORS (sizeof(g_body_colors) / sizeof(g_body_colors[0]))

static void render(rad_wc_surface_t *s, uint32_t frame, uint32_t color_index, int focused) {
    uint32_t *px = s->pixels;
    const uint32_t stride = s->stride;
    const uint32_t body = g_body_colors[color_index % N_BODY_COLORS];

    /* body */
    fill_rect(px, stride, WIN_W, WIN_H, 0, 0, WIN_W, WIN_H, body);
    /* title bar */
    const uint32_t bar = focused ? 0xff2f6fe0u : 0xff394050u;
    fill_rect(px, stride, WIN_W, WIN_H, 0, 0, WIN_W, TITLE_H, bar);
    /* window control dots, top-right, to read as a titled window */
    fill_rect(px, stride, WIN_W, WIN_H, (int)WIN_W - 20, 10, 8, 8, 0xffe05050u);
    fill_rect(px, stride, WIN_W, WIN_H, (int)WIN_W - 36, 10, 8, 8, 0xffe0c050u);
    fill_rect(px, stride, WIN_W, WIN_H, (int)WIN_W - 52, 10, 8, 8, 0xff50c060u);
    /* an "app icon" block on the left of the title bar */
    fill_rect(px, stride, WIN_W, WIN_H, 8, 6, 16, 16, 0xffffffffu);
    fill_rect(px, stride, WIN_W, WIN_H, 11, 9, 10, 10, bar);

    /* live content: a bouncing accent square proves the window is redrawing */
    const int span = (int)WIN_W - 60;
    int t = (int)(frame % (uint32_t)(2 * span));
    if (t >= span) t = 2 * span - t;            /* triangle wave: back and forth */
    const int bx = 20 + t;
    const int by = TITLE_H + 30 + ((int)(frame / 3u) % 40);
    fill_rect(px, stride, WIN_W, WIN_H, bx, by, 26, 26, 0xfff2b53cu);

    /* a progress bar that fills with time then wraps */
    const int barw = (int)((frame % 240u) * (WIN_W - 40) / 240u);
    fill_rect(px, stride, WIN_W, WIN_H, 20, (int)WIN_H - 40, (int)WIN_W - 40, 14, 0xff10131cu);
    fill_rect(px, stride, WIN_W, WIN_H, 20, (int)WIN_H - 40, barw, 14, 0xff4fd08au);

    /* focus border */
    const uint32_t bcol = focused ? 0xff2f6fe0u : 0xff20242cu;
    fill_rect(px, stride, WIN_W, WIN_H, 0, 0, WIN_W, 2, bcol);
    fill_rect(px, stride, WIN_W, WIN_H, 0, (int)WIN_H - 2, WIN_W, 2, bcol);
    fill_rect(px, stride, WIN_W, WIN_H, 0, 0, 2, WIN_H, bcol);
    fill_rect(px, stride, WIN_W, WIN_H, (int)WIN_W - 2, 0, 2, WIN_H, bcol);
}

int main(void) {
    put_str("radwc-demo: starting userland compositor client\n");

    rad_wc_surface_t surface;
    const int rc = rad_wc_surface_open(&surface, "radwc-demo", WIN_W, WIN_H, WIN_X, WIN_Y, WIN_Z);
    if (rc != 0) {
        put_str("radwc-demo: FAILED to open surface\n");
        return 1;
    }
    put_str("radwc-demo: RAD_WC_DEMO_CLIENT_OK window is up\n");
    rad_wc_surface_focus(&surface);

    uint32_t frame = 0;
    uint32_t color_index = 0;
    int focused = 1;
    int dragging = 0;
    int grab_x = 0, grab_y = 0;
    int running = 1;
    int announced_input = 0;

    while (running) {
        rad_wc_input_event_t ev;
        int r;
        while ((r = rad_wc_surface_poll_input(&surface, &ev)) == 1) {
            if (!announced_input) {
                announced_input = 1;
                put_str("radwc-demo: RAD_WC_DEMO_INPUT_OK routed input received\n");
            }
            if (ev.type == RAD_WC_EVENT_POINTER_BUTTON) {
                if (ev.pressed) {
                    focused = 1;
                    if (ev.y < TITLE_H) {          /* grab the title bar */
                        dragging = 1;
                        grab_x = ev.x;
                        grab_y = ev.y;
                    }
                } else {
                    dragging = 0;
                }
            } else if (ev.type == RAD_WC_EVENT_POINTER_MOTION) {
                if (dragging) {
                    /* Keep the grabbed point under the cursor using surface-local
                     * coordinates only: the window has not moved yet this event,
                     * so delta = (new local) - (grab offset). */
                    const int dx = ev.x - grab_x;
                    const int dy = ev.y - grab_y;
                    if (dx != 0 || dy != 0) {
                        rad_wc_surface_set_position(&surface, surface.x + dx, surface.y + dy);
                    }
                }
            } else if (ev.type == RAD_WC_EVENT_KEY) {
                if (ev.pressed) {
                    if (ev.codepoint == 0x1b) {    /* Escape closes the window */
                        running = 0;
                    } else {
                        color_index = (color_index + 1) % N_BODY_COLORS;
                    }
                }
            }
        }
        if (r < 0) break;

        render(&surface, frame, color_index, focused);
        rad_wc_surface_commit(&surface, 0, 0, (int)WIN_W, (int)WIN_H);
        ++frame;
        rad_wc_sleep_ms(16);
    }

    put_str("radwc-demo: closing window\n");
    rad_wc_surface_close(&surface);
    return 0;
}
