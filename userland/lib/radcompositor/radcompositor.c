/* radcompositor - userland client library implementation. Freestanding: talks
 * to the kernel exclusively through rad_syscall6, no libc dependency. */
#include "radcompositor.h"

#include <rad/syscalls.h>

static long wc_open(const char *path, long flags) {
    return rad_syscall6(RAD_WC_SYS_OPEN, (long)path, flags, 0, 0, 0, 0);
}
static long wc_close(long fd) {
    return rad_syscall6(RAD_WC_SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}
static long wc_ioctl(long fd, unsigned long request, void *arg) {
    return rad_syscall6(RAD_WC_SYS_IOCTL, fd, (long)request, (long)arg, 0, 0, 0);
}
static long wc_shm_open(const char *name, long size, long flags) {
    return rad_syscall6(RAD_WC_SYS_SHM_OPEN, (long)name, size, flags, 0, 0, 0);
}
static long wc_mmap(long length, long prot, long flags, long fd) {
    return rad_syscall6(RAD_WC_SYS_MMAP, 0, length, prot, flags, fd, 0);
}

void rad_wc_sleep_ms(uint32_t ms) {
    /* struct timespec { long tv_sec; long tv_nsec; } */
    long ts[2];
    ts[0] = (long)(ms / 1000u);
    ts[1] = (long)((ms % 1000u) * 1000000u);
    rad_syscall6(RAD_WC_SYS_NANOSLEEP, (long)ts, 0, 0, 0, 0, 0);
}

int rad_wc_surface_open(rad_wc_surface_t *surface, const char *shm_name,
                        uint32_t w, uint32_t h, int32_t x, int32_t y, int32_t z) {
    if (!surface || w == 0 || h == 0) return -1;
    for (uint32_t i = 0; i < sizeof(*surface); ++i) ((unsigned char *)surface)[i] = 0;
    surface->compositor_fd = -1;
    surface->shm_fd = -1;

    const uint32_t stride = w;
    const long bytes = (long)stride * (long)h * 4;

    const long shm_fd = wc_shm_open(shm_name, bytes, RAD_WC_MMAP_PROT_READ | RAD_WC_MMAP_PROT_WRITE);
    if (shm_fd < 0) return (int)shm_fd;
    const long map = wc_mmap(bytes, RAD_WC_MMAP_PROT_READ | RAD_WC_MMAP_PROT_WRITE,
                             RAD_WC_MMAP_SHARED, shm_fd);
    if (map < 0) { wc_close(shm_fd); return (int)map; }

    const long comp_fd = wc_open("/dev/compositor0", 2 /* O_RDWR */);
    if (comp_fd < 0) { wc_close(shm_fd); return (int)comp_fd; }

    rad_wc_ipc_surface_t req;
    for (uint32_t i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
    req.size = sizeof(req);
    req.shm_fd = (int32_t)shm_fd;
    req.width = w;
    req.height = h;
    req.stride_pixels = stride;
    req.x = x;
    req.y = y;
    req.z = z;
    req.flags = 0;
    const long rc = wc_ioctl(comp_fd, RAD_WC_IOCTL_CREATE_SURFACE, &req);
    if (rc != 0) { wc_close(comp_fd); wc_close(shm_fd); return (int)rc; }

    surface->compositor_fd = (int)comp_fd;
    surface->shm_fd = (int)shm_fd;
    surface->pixels = (uint32_t *)(unsigned long)map;
    surface->surface_id = req.surface_id;
    surface->width = w;
    surface->height = h;
    surface->stride = stride;
    surface->x = x;
    surface->y = y;
    surface->z = z;
    return 0;
}

void rad_wc_surface_commit(rad_wc_surface_t *surface, int32_t x, int32_t y,
                           int32_t w, int32_t h) {
    if (!surface || surface->compositor_fd < 0) return;
    rad_wc_ipc_damage_t dmg;
    for (uint32_t i = 0; i < sizeof(dmg); ++i) ((unsigned char *)&dmg)[i] = 0;
    dmg.size = sizeof(dmg);
    dmg.surface_id = surface->surface_id;
    dmg.x = x;
    dmg.y = y;
    dmg.width = w;
    dmg.height = h;
    dmg.flags = 0;
    wc_ioctl(surface->compositor_fd, RAD_WC_IOCTL_QUEUE_DAMAGE, &dmg);
}

int rad_wc_surface_set_position(rad_wc_surface_t *surface, int32_t x, int32_t y) {
    if (!surface || surface->compositor_fd < 0) return -1;
    rad_wc_ipc_surface_t req;
    for (uint32_t i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
    req.size = sizeof(req);
    req.surface_id = surface->surface_id;
    req.x = x;
    req.y = y;
    req.width = surface->width;
    req.height = surface->height;
    const long rc = wc_ioctl(surface->compositor_fd, RAD_WC_IOCTL_SET_BOUNDS, &req);
    if (rc == 0) { surface->x = x; surface->y = y; }
    return (int)rc;
}

int rad_wc_surface_focus(rad_wc_surface_t *surface) {
    if (!surface || surface->compositor_fd < 0) return -1;
    rad_wc_ipc_surface_t req;
    for (uint32_t i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
    req.size = sizeof(req);
    req.surface_id = surface->surface_id;
    return (int)wc_ioctl(surface->compositor_fd, RAD_WC_IOCTL_FOCUS, &req);
}

int rad_wc_surface_poll_input(rad_wc_surface_t *surface, rad_wc_input_event_t *out) {
    if (!surface || surface->compositor_fd < 0) return -1;
    rad_wc_ipc_input_t poll;
    for (uint32_t i = 0; i < sizeof(poll); ++i) ((unsigned char *)&poll)[i] = 0;
    poll.size = sizeof(poll);
    poll.surface_id = surface->surface_id;
    const long rc = wc_ioctl(surface->compositor_fd, RAD_WC_IOCTL_POLL_INPUT, &poll);
    if (rc != 0) return (int)rc;
    if (!poll.has_event) return 0;
    if (out) *out = poll.event;
    return 1;
}

void rad_wc_surface_close(rad_wc_surface_t *surface) {
    if (!surface) return;
    if (surface->compositor_fd >= 0) {
        rad_wc_ipc_surface_t req;
        for (uint32_t i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
        req.size = sizeof(req);
        req.surface_id = surface->surface_id;
        wc_ioctl(surface->compositor_fd, RAD_WC_IOCTL_DESTROY_SURFACE, &req);
        wc_close(surface->compositor_fd);
        surface->compositor_fd = -1;
    }
    if (surface->shm_fd >= 0) {
        wc_close(surface->shm_fd);
        surface->shm_fd = -1;
    }
}
