#include "RADCompositorCore.h"

#include <string.h>

namespace {

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    const size_t len = strlen(src);
    const size_t copy = len < dst_size - 1u ? len : dst_size - 1u;
    if (copy) memcpy(dst, src, copy);
    dst[copy] = '\0';
}

rad_compositor_surface_info_t *find_surface(rad_compositor_t *compositor, uint32_t surface_id) {
    if (!compositor || surface_id == 0) return nullptr;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        if (compositor->surfaces[i].id == surface_id) return &compositor->surfaces[i];
    }
    return nullptr;
}

const rad_compositor_surface_info_t *find_surface_const(const rad_compositor_t *compositor, uint32_t surface_id) {
    return find_surface(const_cast<rad_compositor_t*>(compositor), surface_id);
}

int contains_point(const rad_compositor_surface_info_t& surface, int32_t x, int32_t y) {
    return surface.visible
        && x >= surface.bounds.x
        && y >= surface.bounds.y
        && x < surface.bounds.x + surface.bounds.width
        && y < surface.bounds.y + surface.bounds.height;
}

int rect_intersect(const rad_compositor_rect_t& a, const rad_compositor_rect_t& b, rad_compositor_rect_t *out) {
    const int32_t x0 = a.x > b.x ? a.x : b.x;
    const int32_t y0 = a.y > b.y ? a.y : b.y;
    const int32_t x1 = (a.x + a.width) < (b.x + b.width) ? (a.x + a.width) : (b.x + b.width);
    const int32_t y1 = (a.y + a.height) < (b.y + b.height) ? (a.y + a.height) : (b.y + b.height);
    if (x1 <= x0 || y1 <= y0) return 0;
    if (out) {
        out->x = x0;
        out->y = y0;
        out->width = x1 - x0;
        out->height = y1 - y0;
    }
    return 1;
}

rad_compositor_rect_t framebuffer_rect(const rad_compositor_t *compositor) {
    rad_compositor_rect_t rect{};
    if (!compositor) return rect;
    rect.width = static_cast<int32_t>(compositor->framebuffer_width);
    rect.height = static_cast<int32_t>(compositor->framebuffer_height);
    return rect;
}

uint32_t blend_pixel(uint32_t dst, uint32_t src) {
    const uint32_t alpha = (src >> 24u) & 0xffu;
    if (alpha == 0xffu) return src;
    if (alpha == 0u) return dst;
    const uint32_t inv = 255u - alpha;
    const uint32_t sr = (src >> 16u) & 0xffu;
    const uint32_t sg = (src >> 8u) & 0xffu;
    const uint32_t sb = src & 0xffu;
    const uint32_t dr = (dst >> 16u) & 0xffu;
    const uint32_t dg = (dst >> 8u) & 0xffu;
    const uint32_t db = dst & 0xffu;
    const uint32_t r = (sr * alpha + dr * inv + 127u) / 255u;
    const uint32_t g = (sg * alpha + dg * inv + 127u) / 255u;
    const uint32_t b = (sb * alpha + db * inv + 127u) / 255u;
    return 0xff000000u | (r << 16u) | (g << 8u) | b;
}

void copy_forward_rect(rad_compositor_t *compositor, const rad_compositor_rect_t& rect) {
    if (!compositor || !compositor->framebuffer) return;
    if (!compositor->frontbuffer) {
        for (int32_t row = 0; row < rect.height; ++row) {
            uint32_t *dst = compositor->framebuffer
                + static_cast<size_t>(rect.y + row) * compositor->framebuffer_stride_pixels
                + static_cast<size_t>(rect.x);
            for (int32_t col = 0; col < rect.width; ++col) dst[col] = compositor->clear_color;
        }
        return;
    }
    for (int32_t row = 0; row < rect.height; ++row) {
        uint32_t *dst = compositor->framebuffer
            + static_cast<size_t>(rect.y + row) * compositor->framebuffer_stride_pixels
            + static_cast<size_t>(rect.x);
        const uint32_t *src = compositor->frontbuffer
            + static_cast<size_t>(rect.y + row) * compositor->frontbuffer_stride_pixels
            + static_cast<size_t>(rect.x);
        memcpy(dst, src, static_cast<size_t>(rect.width) * sizeof(uint32_t));
    }
}

void blit_surface_rect(rad_compositor_t *compositor, const rad_compositor_surface_info_t& surface, const rad_compositor_rect_t& damage) {
    if (!compositor || !surface.visible || !surface.pixels || surface.bounds.width <= 0 || surface.bounds.height <= 0) return;
    rad_compositor_rect_t clipped{};
    if (!rect_intersect(surface.bounds, damage, &clipped)) return;
    rad_compositor_rect_t fb{};
    if (!rect_intersect(clipped, framebuffer_rect(compositor), &fb)) return;
    const int32_t src_x0 = fb.x - surface.bounds.x;
    const int32_t src_y0 = fb.y - surface.bounds.y;

    const int alpha = (surface.flags & RAD_COMPOSITOR_SURFACE_ALPHA) != 0;
    for (int32_t row = 0; row < fb.height; ++row) {
        uint32_t *dst = compositor->framebuffer
            + static_cast<size_t>(fb.y + row) * compositor->framebuffer_stride_pixels
            + static_cast<size_t>(fb.x);
        const uint32_t *src = surface.pixels
            + static_cast<size_t>(src_y0 + row) * surface.stride_pixels
            + static_cast<size_t>(src_x0);
        if (!alpha) {
            memcpy(dst, src, static_cast<size_t>(fb.width) * sizeof(uint32_t));
        } else {
            for (int32_t col = 0; col < fb.width; ++col) dst[col] = blend_pixel(dst[col], src[col]);
        }
    }
}

void blit_surface(rad_compositor_t *compositor, const rad_compositor_surface_info_t& surface) {
    blit_surface_rect(compositor, surface, surface.bounds);
}

void queue_surface_full_damage(rad_compositor_t *compositor, const rad_compositor_surface_info_t& surface, uint32_t flags) {
    if (!compositor || !surface.id) return;
    rad_compositor_rect_t rect{};
    if (flags & RAD_COMPOSITOR_DAMAGE_EXPOSED) {
        rect = surface.bounds;
    } else {
        rect.width = surface.bounds.width;
        rect.height = surface.bounds.height;
    }
    rad_compositor_queue_damage(compositor, surface.id, &rect, flags);
}

bool rect_contains(const rad_compositor_rect_t& outer, const rad_compositor_rect_t& inner) {
    return inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

// Highest z of a visible, opaque surface whose bounds fully contain `rect`. Such a
// surface fully paints the rect, so everything strictly below it is occluded (drawing
// it would be immediately overwritten) and the copy-forward for this rect is wasted.
// Returns INT32_MIN with *found=false when nothing fully covers the rect.
int32_t opaque_cover_floor_z(const rad_compositor_t *compositor, const rad_compositor_rect_t& rect, bool *found) {
    int32_t floor_z = INT32_MIN;
    bool any = false;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        const rad_compositor_surface_info_t& s = compositor->surfaces[i];
        if (!s.id || !s.visible || !s.pixels) continue;
        if (s.flags & RAD_COMPOSITOR_SURFACE_ALPHA) continue;   // translucent: does not occlude
        if (!rect_contains(s.bounds, rect)) continue;
        if (!any || s.z > floor_z) { floor_z = s.z; any = true; }
    }
    if (found) *found = any;
    return floor_z;
}

void draw_surfaces_for_rect(rad_compositor_t *compositor, const rad_compositor_rect_t& rect) {
    bool covered = false;
    const int32_t floor_z = opaque_cover_floor_z(compositor, rect, &covered);
    // Start above any fully-covering opaque surface: strictly lower surfaces are occluded.
    int32_t last_z = covered ? floor_z : INT32_MIN;
    for (;;) {
        const rad_compositor_surface_info_t *next = nullptr;
        for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
            const rad_compositor_surface_info_t& surface = compositor->surfaces[i];
            if (!surface.id || !surface.visible || surface.z < last_z) continue;
            if (!rect_intersect(surface.bounds, rect, nullptr)) continue;
            if (surface.z == last_z && next) continue;
            if (!next || surface.z < next->z) next = &surface;
        }
        if (!next) break;
        blit_surface_rect(compositor, *next, rect);
        last_z = next->z + 1;
    }
}

} // namespace

extern "C" {

rad_status_t rad_compositor_init(rad_compositor_t *compositor, uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t stride_pixels) {
    if (!compositor || !framebuffer || width == 0 || height == 0 || stride_pixels < width) return RAD_STATUS_INVALID_ARGUMENT;
    memset(compositor, 0, sizeof(*compositor));
    compositor->framebuffer = framebuffer;
    compositor->frontbuffer = nullptr;
    compositor->framebuffer_width = width;
    compositor->framebuffer_height = height;
    compositor->framebuffer_stride_pixels = stride_pixels;
    compositor->frontbuffer_stride_pixels = stride_pixels;
    compositor->clear_color = 0xff07111fu;
    compositor->next_surface_id = 1u;
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_set_framebuffers(rad_compositor_t *compositor, uint32_t *frontbuffer, uint32_t front_stride_pixels, uint32_t *backbuffer, uint32_t back_stride_pixels) {
    if (!compositor || !backbuffer || back_stride_pixels < compositor->framebuffer_width) return RAD_STATUS_INVALID_ARGUMENT;
    if (frontbuffer && front_stride_pixels < compositor->framebuffer_width) return RAD_STATUS_INVALID_ARGUMENT;
    compositor->frontbuffer = frontbuffer;
    compositor->frontbuffer_stride_pixels = frontbuffer ? front_stride_pixels : back_stride_pixels;
    compositor->framebuffer = backbuffer;
    compositor->framebuffer_stride_pixels = back_stride_pixels;
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_create_surface(rad_compositor_t *compositor, const rad_compositor_surface_config_t *config, uint32_t *surface_id) {
    if (!compositor || !config || !surface_id || !config->pixels || config->width <= 0 || config->height <= 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (config->stride_pixels < static_cast<uint32_t>(config->width)) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        rad_compositor_surface_info_t& surface = compositor->surfaces[i];
        if (surface.id != 0) continue;
        memset(&surface, 0, sizeof(surface));
        surface.id = compositor->next_surface_id++;
        if (compositor->next_surface_id == 0) compositor->next_surface_id = 1u;
        copy_string(surface.app_id, sizeof(surface.app_id), config->app_id);
        copy_string(surface.title, sizeof(surface.title), config->title);
        surface.bounds.x = config->x;
        surface.bounds.y = config->y;
        surface.bounds.width = config->width;
        surface.bounds.height = config->height;
        surface.z = config->z;
        surface.flags = config->flags;
        surface.visible = 1;
        surface.dirty = 1;
        surface.pixels = config->pixels;
        surface.stride_pixels = config->stride_pixels;
        *surface_id = surface.id;
        queue_surface_full_damage(compositor, surface, 0);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_compositor_destroy_surface(rad_compositor_t *compositor, uint32_t surface_id) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface) return RAD_STATUS_NOT_FOUND;
    memset(surface, 0, sizeof(*surface));
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_set_surface_bounds(rad_compositor_t *compositor, uint32_t surface_id, const rad_compositor_rect_t *bounds) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface || !bounds || bounds->width <= 0 || bounds->height <= 0) return RAD_STATUS_INVALID_ARGUMENT;
    const rad_compositor_rect_t old_bounds = surface->bounds;
    surface->bounds = *bounds;
    surface->dirty = 1;
    if (rect_intersect(old_bounds, *bounds, nullptr)) {
        // Overlapping old/new footprints (a drag or small move/resize): damage their union
        // as a single rect instead of the full old rect plus the full new rect. Those two
        // overlap ~99% during a drag, so composing and presenting them separately doubles
        // the per-frame work; the union is one contiguous rect -- composed and presented
        // once, with the exposed area restored as part of the same pass. Queued as an
        // absolute rect (EXPOSED = stored as-is rather than offset by the new bounds).
        const int32_t x0 = old_bounds.x < bounds->x ? old_bounds.x : bounds->x;
        const int32_t y0 = old_bounds.y < bounds->y ? old_bounds.y : bounds->y;
        const int32_t ox1 = old_bounds.x + old_bounds.width, nx1 = bounds->x + bounds->width;
        const int32_t oy1 = old_bounds.y + old_bounds.height, ny1 = bounds->y + bounds->height;
        rad_compositor_rect_t u{};
        u.x = x0;
        u.y = y0;
        u.width = (ox1 > nx1 ? ox1 : nx1) - x0;
        u.height = (oy1 > ny1 ? oy1 : ny1) - y0;
        rad_compositor_queue_damage(compositor, surface_id, &u, RAD_COMPOSITOR_DAMAGE_EXPOSED);
    } else {
        // Disjoint footprints (a teleport): keep two tight rects so the empty gap between
        // the old and new positions is never needlessly composited.
        rad_compositor_queue_damage(compositor, surface_id, &old_bounds, RAD_COMPOSITOR_DAMAGE_EXPOSED);
        queue_surface_full_damage(compositor, *surface, 0);
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_focus_surface(rad_compositor_t *compositor, uint32_t surface_id) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface) return RAD_STATUS_NOT_FOUND;
    int32_t max_z = surface->z;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        if (compositor->surfaces[i].id) {
            compositor->surfaces[i].focused = 0;
            if (compositor->surfaces[i].z > max_z) max_z = compositor->surfaces[i].z;
        }
    }
    surface->focused = 1;
    surface->z = max_z + 1;
    surface->dirty = 1;
    queue_surface_full_damage(compositor, *surface, 0);
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_set_surface_visible(rad_compositor_t *compositor, uint32_t surface_id, int visible) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface) return RAD_STATUS_NOT_FOUND;
    surface->visible = visible ? 1 : 0;
    surface->dirty = 1;
    queue_surface_full_damage(compositor, *surface, visible ? 0 : RAD_COMPOSITOR_DAMAGE_EXPOSED);
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_mark_surface_dirty(rad_compositor_t *compositor, uint32_t surface_id) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface) return RAD_STATUS_NOT_FOUND;
    surface->dirty = 1;
    return rad_compositor_queue_damage(compositor, surface_id, &surface->bounds, 0);
}

rad_status_t rad_compositor_queue_damage(rad_compositor_t *compositor, uint32_t surface_id, const rad_compositor_rect_t *rect, uint32_t flags) {
    if (!compositor || !rect || rect->width <= 0 || rect->height <= 0) return RAD_STATUS_INVALID_ARGUMENT;
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface) return RAD_STATUS_NOT_FOUND;
    const uint32_t index = (compositor->damage_head + compositor->damage_count) % RAD_COMPOSITOR_MAX_DAMAGE;
    if (compositor->damage_count >= RAD_COMPOSITOR_MAX_DAMAGE) {
        compositor->damage_head = 0;
        compositor->damage_count = 1;
        compositor->damage[0].surface_id = surface_id;
        compositor->damage[0].rect = surface->bounds;
        compositor->damage[0].flags = flags;
        return RAD_STATUS_OK;
    }
    compositor->damage[index].surface_id = surface_id;
    if (flags & RAD_COMPOSITOR_DAMAGE_EXPOSED) {
        compositor->damage[index].rect = *rect;
    } else {
        compositor->damage[index].rect.x = surface->bounds.x + rect->x;
        compositor->damage[index].rect.y = surface->bounds.y + rect->y;
        compositor->damage[index].rect.width = rect->width;
        compositor->damage[index].rect.height = rect->height;
    }
    compositor->damage[index].flags = flags;
    ++compositor->damage_count;
    surface->dirty = 1;
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_submit_surface_pixels(rad_compositor_t *compositor, uint32_t surface_id, uint32_t *pixels, uint32_t stride_pixels) {
    rad_compositor_surface_info_t *surface = find_surface(compositor, surface_id);
    if (!surface || !pixels || stride_pixels < static_cast<uint32_t>(surface->bounds.width)) return RAD_STATUS_INVALID_ARGUMENT;
    surface->pixels = pixels;
    surface->stride_pixels = stride_pixels;
    surface->dirty = 1;
    queue_surface_full_damage(compositor, *surface, 0);
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_compose_frame(rad_compositor_t *compositor) {
    if (!compositor || !compositor->framebuffer) return RAD_STATUS_INVALID_ARGUMENT;
    compositor->last_present_rect_count = 0;
    if (compositor->damage_count == 0) return RAD_STATUS_OK;

    const uint32_t count = compositor->damage_count;
    for (uint32_t n = 0; n < count; ++n) {
        const uint32_t index = (compositor->damage_head + n) % RAD_COMPOSITOR_MAX_DAMAGE;
        rad_compositor_rect_t clipped{};
        if (!rect_intersect(compositor->damage[index].rect, framebuffer_rect(compositor), &clipped)) continue;
        // A rect fully covered by an opaque surface is entirely repainted below, so the
        // copy-forward memcpy would be 100% overwritten -- skip it. Identical pixels, less
        // memory traffic (the dominant per-drag-frame cost).
        bool covered = false;
        opaque_cover_floor_z(compositor, clipped, &covered);
        if (!covered) copy_forward_rect(compositor, clipped);
        draw_surfaces_for_rect(compositor, clipped);
        if (compositor->last_present_rect_count < RAD_COMPOSITOR_MAX_DAMAGE) {
            compositor->last_present_rects[compositor->last_present_rect_count] = clipped;
            ++compositor->last_present_rect_count;
        }
    }
    compositor->damage_head = 0;
    compositor->damage_count = 0;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        if (compositor->surfaces[i].id) compositor->surfaces[i].dirty = 0;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_hit_test(const rad_compositor_t *compositor, int32_t global_x, int32_t global_y, rad_compositor_input_result_t *result) {
    if (!compositor || !result) return RAD_STATUS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    const rad_compositor_surface_info_t *best = nullptr;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        const rad_compositor_surface_info_t& surface = compositor->surfaces[i];
        if (!surface.id || !contains_point(surface, global_x, global_y)) continue;
        if (!best || surface.z > best->z) best = &surface;
    }
    if (!best) return RAD_STATUS_NOT_FOUND;
    result->surface_id = best->id;
    result->local_x = global_x - best->bounds.x;
    result->local_y = global_y - best->bounds.y;
    result->hit = 1;
    return RAD_STATUS_OK;
}

rad_status_t rad_compositor_dispatch_input(const rad_compositor_t *compositor, const rad_input_event_t *event, rad_compositor_input_result_t *result) {
    if (!event || !result) return RAD_STATUS_INVALID_ARGUMENT;
    if (event->type == RAD_INPUT_EVENT_POINTER_MOTION
        || event->type == RAD_INPUT_EVENT_POINTER_BUTTON
        || event->type == RAD_INPUT_EVENT_POINTER_SCROLL) {
        return rad_compositor_hit_test(compositor, event->x, event->y, result);
    }
    memset(result, 0, sizeof(*result));
    const rad_compositor_surface_info_t *focused = nullptr;
    if (compositor) {
        for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
            if (compositor->surfaces[i].id && compositor->surfaces[i].focused) {
                focused = &compositor->surfaces[i];
                break;
            }
        }
    }
    if (!focused) return RAD_STATUS_NOT_FOUND;
    result->surface_id = focused->id;
    result->hit = 1;
    return RAD_STATUS_OK;
}

size_t rad_compositor_list_surfaces(const rad_compositor_t *compositor, rad_compositor_surface_info_t *surfaces, size_t capacity) {
    if (!compositor) return 0;
    size_t count = 0;
    for (size_t i = 0; i < RAD_COMPOSITOR_MAX_SURFACES; ++i) {
        if (!compositor->surfaces[i].id) continue;
        if (surfaces && count < capacity) surfaces[count] = compositor->surfaces[i];
        ++count;
    }
    return count;
}

}
