/* radcompositor - userland client library for the RADPx-OS compositor.
 *
 * A client renders pixels into a shared-memory buffer and hands the buffer to
 * the kernel compositor as a surface over /dev/compositor0. The compositor owns
 * the screen, z-order, focus and input routing; the client owns its pixels.
 *
 * This header mirrors the wire ABI declared in <radkernel/radkernel.h>. It is
 * kept self-contained so a freestanding client (no kernel headers) can link it.
 */
#ifndef RAD_COMPOSITOR_CLIENT_H
#define RAD_COMPOSITOR_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- syscall numbers (subset; see <rad/syscalls.h>) ----------------------- */
#define RAD_WC_SYS_WRITE 1
#define RAD_WC_SYS_OPEN 2
#define RAD_WC_SYS_CLOSE 3
#define RAD_WC_SYS_IOCTL 4
#define RAD_WC_SYS_NANOSLEEP 9
#define RAD_WC_SYS_MMAP 35
#define RAD_WC_SYS_SHM_OPEN 37
#define RAD_WC_SYS_SHM_UNLINK 38

#define RAD_WC_MMAP_PROT_READ 1u
#define RAD_WC_MMAP_PROT_WRITE 2u
#define RAD_WC_MMAP_SHARED 1u

/* --- ioctl encoding (mirrors radkernel.h) --------------------------------- */
#define RAD_WC_IOCTL(dir, type, nr, size) \
    ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | \
     (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_WC_IOCTL_WRITE 1u
#define RAD_WC_IOCTL_READWRITE 3u
#define RAD_WC_IOCTL_TYPE 'C'

/* --- wire structs (must match radkernel.h byte-for-byte) ------------------ */
typedef struct rad_wc_ipc_surface {
    uint32_t size;
    int32_t shm_fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
    int32_t x;
    int32_t y;
    int32_t z;
    uint32_t flags;
    uint32_t surface_id;
} rad_wc_ipc_surface_t;

typedef struct rad_wc_ipc_damage {
    uint32_t size;
    uint32_t surface_id;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    uint32_t flags;
} rad_wc_ipc_damage_t;

/* Input event, surface-local for pointer events. Matches rad_input_event_t. */
typedef struct rad_wc_input_event {
    uint32_t size;
    uint32_t type;       /* rad_input_event_type_t */
    uint32_t code;
    uint32_t codepoint;
    uint32_t modifiers;
    uint8_t pressed;
    uint8_t repeat;
    uint16_t reserved;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    int32_t scroll_x;
    int32_t scroll_y;
    uint32_t buttons;
} rad_wc_input_event_t;

typedef struct rad_wc_ipc_input {
    uint32_t size;
    uint32_t surface_id;
    uint32_t has_event;
    uint32_t reserved;
    rad_wc_input_event_t event;
} rad_wc_ipc_input_t;

/* rad_input_event_type_t values used by clients. */
#define RAD_WC_EVENT_KEY 1u
#define RAD_WC_EVENT_POINTER_MOTION 2u
#define RAD_WC_EVENT_POINTER_BUTTON 3u
#define RAD_WC_EVENT_POINTER_SCROLL 4u

#define RAD_WC_IOCTL_CREATE_SURFACE  RAD_WC_IOCTL(RAD_WC_IOCTL_READWRITE, RAD_WC_IOCTL_TYPE, 1u, sizeof(rad_wc_ipc_surface_t))
#define RAD_WC_IOCTL_QUEUE_DAMAGE    RAD_WC_IOCTL(RAD_WC_IOCTL_WRITE,     RAD_WC_IOCTL_TYPE, 2u, sizeof(rad_wc_ipc_damage_t))
#define RAD_WC_IOCTL_DESTROY_SURFACE RAD_WC_IOCTL(RAD_WC_IOCTL_WRITE,     RAD_WC_IOCTL_TYPE, 3u, sizeof(rad_wc_ipc_surface_t))
#define RAD_WC_IOCTL_SET_BOUNDS      RAD_WC_IOCTL(RAD_WC_IOCTL_WRITE,     RAD_WC_IOCTL_TYPE, 4u, sizeof(rad_wc_ipc_surface_t))
#define RAD_WC_IOCTL_FOCUS           RAD_WC_IOCTL(RAD_WC_IOCTL_WRITE,     RAD_WC_IOCTL_TYPE, 5u, sizeof(rad_wc_ipc_surface_t))
#define RAD_WC_IOCTL_POLL_INPUT      RAD_WC_IOCTL(RAD_WC_IOCTL_READWRITE, RAD_WC_IOCTL_TYPE, 6u, sizeof(rad_wc_ipc_input_t))

/* --- client handles ------------------------------------------------------- */
typedef struct rad_wc_surface {
    int compositor_fd;
    int shm_fd;
    uint32_t *pixels;     /* mapped shared buffer, width*height XRGB8888 */
    uint32_t surface_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;      /* in pixels */
    int32_t x;
    int32_t y;
    int32_t z;
} rad_wc_surface_t;

/* Open a window surface: allocate shm named @shm_name, map it, and register it
 * with the compositor at (x,y) size (w,h) and z-order @z. Returns 0 on success,
 * a negative status on failure. On success @surface->pixels is a writable
 * width*height XRGB8888 buffer. */
int rad_wc_surface_open(rad_wc_surface_t *surface, const char *shm_name,
                        uint32_t w, uint32_t h, int32_t x, int32_t y, int32_t z);

/* Tell the compositor that a rectangle of the surface changed. */
void rad_wc_surface_commit(rad_wc_surface_t *surface, int32_t x, int32_t y,
                           int32_t w, int32_t h);

/* Move/resize the surface on screen (pixels/stride unchanged). */
int rad_wc_surface_set_position(rad_wc_surface_t *surface, int32_t x, int32_t y);

/* Raise + focus the surface. */
int rad_wc_surface_focus(rad_wc_surface_t *surface);

/* Dequeue one routed input event. Returns 1 and fills @out when an event was
 * waiting, 0 when the queue is empty, negative on error. */
int rad_wc_surface_poll_input(rad_wc_surface_t *surface, rad_wc_input_event_t *out);

/* Destroy the surface and release its resources. */
void rad_wc_surface_close(rad_wc_surface_t *surface);

/* Sleep for @ms milliseconds (frame pacing helper). */
void rad_wc_sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* RAD_COMPOSITOR_CLIENT_H */
