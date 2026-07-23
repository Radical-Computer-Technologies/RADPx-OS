/* radwc-demo - a userland RADPx-OS application that renders into a shared-memory
 * surface composited by the kernel, and reacts to input routed back from the
 * compositor. It is the reference client for the client/server compositor path:
 * a separate process (not kernel code) owning a real, movable, interactive
 * window.
 *
 *   - creates a window surface over shared memory (/dev/compositor0)
 *   - draws a titled window with real text (a 5x7 bitmap font): its own PID,
 *     a live frame counter, and instructions, plus a live-animated body
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

/* A compact 5x7 bitmap font: each glyph is 7 rows, low 5 bits = columns (MSB
 * left). Index order: space, A-Z, 0-9, ':' '.' '-' '/' '('. */
static const unsigned char g_font5x7[][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, /* : */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x01,0x02,0x04,0x04,0x08,0x10,0x10}, /* / */
};

static int glyph_index(char c) {
    if (c == ' ') return 0;
    if (c >= 'A' && c <= 'Z') return 1 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 1 + (c - 'a');
    if (c >= '0' && c <= '9') return 27 + (c - '0');
    if (c == ':') return 37;
    if (c == '.') return 38;
    if (c == '-') return 39;
    if (c == '/') return 40;
    return 0;
}

/* Draw text at (x,y) with an integer pixel scale. Returns the x past the text. */
static int draw_text(uint32_t *px, uint32_t stride, uint32_t win_w, uint32_t win_h,
                     int x, int y, const char *s, int scale, uint32_t color) {
    for (const char *p = s; *p; ++p) {
        const unsigned char *g = g_font5x7[glyph_index(*p)];
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (g[row] & (1u << (4 - col))) {
                    fill_rect(px, stride, win_w, win_h,
                              x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += (5 + 1) * scale;   /* glyph width + 1px gap */
    }
    return x;
}

/* Format an unsigned integer into buf (decimal), return length. */
static int u_to_str(char *buf, unsigned long v) {
    char tmp[20];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    buf[n] = 0;
    return n;
}

/* XRGB8888 opaque colors (high byte = 0xff = fully opaque). */
static const uint32_t g_body_colors[] = {
    0xff1e2a44u, 0xff203a2eu, 0xff3a2440u, 0xff402a1eu, 0xff243a44u,
};
#define N_BODY_COLORS (sizeof(g_body_colors) / sizeof(g_body_colors[0]))

static void render(rad_wc_surface_t *s, uint32_t frame, uint32_t color_index,
                   int focused, long pid) {
    uint32_t *px = s->pixels;
    const uint32_t stride = s->stride;
    const uint32_t body = g_body_colors[color_index % N_BODY_COLORS];
    const uint32_t ink = 0xffe8ecf4u;
    const uint32_t dim = 0xff8791a5u;

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
    /* title text */
    draw_text(px, stride, WIN_W, WIN_H, 30, 10, "RAD CLIENT", 2, 0xffffffffu);

    /* live info lines proving this is a real, separate userland process */
    char line[48];
    int n = 0;
    for (const char *p = "PID: "; *p; ++p) line[n++] = *p;
    n += u_to_str(line + n, (unsigned long)pid);
    line[n] = 0;
    draw_text(px, stride, WIN_W, WIN_H, 20, TITLE_H + 16, "USERLAND PROCESS", 2, ink);
    draw_text(px, stride, WIN_W, WIN_H, 20, TITLE_H + 44, line, 2, dim);

    n = 0;
    for (const char *p = "FRAME: "; *p; ++p) line[n++] = *p;
    n += u_to_str(line + n, frame);
    line[n] = 0;
    draw_text(px, stride, WIN_W, WIN_H, 20, TITLE_H + 68, line, 2, dim);
    draw_text(px, stride, WIN_W, WIN_H, 20, TITLE_H + 96, "DRAG BAR - KEY CYCLES", 1, dim);
    draw_text(px, stride, WIN_W, WIN_H, 20, TITLE_H + 110, "COLOR - ESC CLOSES", 1, dim);

    /* live content: a bouncing accent square proves the window is redrawing */
    const int span = (int)WIN_W - 60;
    int t = (int)(frame % (uint32_t)(2 * span));
    if (t >= span) t = 2 * span - t;            /* triangle wave: back and forth */
    const int bx = 20 + t;
    const int by = (int)WIN_H - 92;
    fill_rect(px, stride, WIN_W, WIN_H, bx, by, 22, 22, 0xfff2b53cu);

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
    /* Do NOT steal keyboard focus on launch -- a client takes keyboard focus
     * only when the user clicks it, so the WM (login, terminal, ...) keeps input
     * by default. */

    const long pid = rad_syscall6(14 /* getpid */, 0, 0, 0, 0, 0, 0);

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

        render(&surface, frame, color_index, focused, pid);
        rad_wc_surface_commit(&surface, 0, 0, (int)WIN_W, (int)WIN_H);
        ++frame;
        rad_wc_sleep_ms(16);
    }

    put_str("radwc-demo: closing window\n");
    rad_wc_surface_close(&surface);
    return 0;
}
