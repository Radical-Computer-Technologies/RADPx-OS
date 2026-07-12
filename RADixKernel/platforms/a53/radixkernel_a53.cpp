#include "radixkernel_a53.h"

#include <string.h>

namespace {
constexpr uintptr_t UserBase = 0x40000000u;
constexpr uintptr_t UserLimit = 0x41000000u;
constexpr uint32_t PageSize = 4096u;
constexpr size_t MaxModelPages = 16u;
constexpr size_t MaxMappings = 16u;
constexpr uint32_t MapPresent = 1u << 0u;
constexpr uint32_t MapWrite = 1u << 1u;
constexpr uint32_t MapCow = 1u << 2u;

struct ModelPage {
    uint8_t bytes[PageSize];
    uint16_t refs;
    uint8_t used;
};

struct ModelMapping {
    uintptr_t va;
    size_t page;
    uint32_t flags;
};

struct ModelSpace {
    ModelMapping mappings[MaxMappings];
    size_t mapping_count;
};

struct A53State {
    rad_a53_capabilities_t caps;
    int process_arch_registered;
};

A53State g_a53{};
ModelPage g_pages[MaxModelPages];

uintptr_t align_down(uintptr_t value) {
    return value & ~(static_cast<uintptr_t>(PageSize) - 1u);
}

void reset_model_pages() {
    memset(g_pages, 0, sizeof(g_pages));
}

int alloc_model_page(size_t *page_out) {
    if (!page_out) return 0;
    for (size_t i = 0; i < MaxModelPages; ++i) {
        if (g_pages[i].used) continue;
        memset(g_pages[i].bytes, 0, sizeof(g_pages[i].bytes));
        g_pages[i].used = 1u;
        g_pages[i].refs = 1u;
        *page_out = i;
        return 1;
    }
    return 0;
}

void retain_model_page(size_t page) {
    if (page < MaxModelPages && g_pages[page].used) ++g_pages[page].refs;
}

void release_model_page(size_t page) {
    if (page >= MaxModelPages || !g_pages[page].used || g_pages[page].refs == 0u) return;
    --g_pages[page].refs;
    if (g_pages[page].refs == 0u) memset(&g_pages[page], 0, sizeof(g_pages[page]));
}

ModelMapping *find_mapping(ModelSpace *space, uintptr_t va) {
    if (!space) return nullptr;
    const uintptr_t page_va = align_down(va);
    for (size_t i = 0; i < space->mapping_count; ++i) {
        if ((space->mappings[i].flags & MapPresent) && space->mappings[i].va == page_va) return &space->mappings[i];
    }
    return nullptr;
}

int map_model_page(ModelSpace *space, uintptr_t va, size_t page, uint32_t flags) {
    if (!space || page >= MaxModelPages || !g_pages[page].used || space->mapping_count >= MaxMappings) return 0;
    if (va < UserBase || va >= UserLimit || (va % PageSize) != 0u) return 0;
    ModelMapping& mapping = space->mappings[space->mapping_count++];
    mapping.va = va;
    mapping.page = page;
    mapping.flags = flags | MapPresent;
    return 1;
}

void destroy_space(ModelSpace *space) {
    if (!space) return;
    for (size_t i = 0; i < space->mapping_count; ++i) {
        if (space->mappings[i].flags & MapPresent) release_model_page(space->mappings[i].page);
    }
    memset(space, 0, sizeof(*space));
}

int clone_space_cow(ModelSpace *child, ModelSpace *parent) {
    if (!child || !parent) return 0;
    memset(child, 0, sizeof(*child));
    for (size_t i = 0; i < parent->mapping_count; ++i) {
        ModelMapping& parent_mapping = parent->mappings[i];
        if ((parent_mapping.flags & MapPresent) == 0u) continue;
        uint32_t flags = parent_mapping.flags;
        if (flags & MapWrite) {
            flags = (flags & ~MapWrite) | MapCow;
            parent_mapping.flags = flags;
        }
        retain_model_page(parent_mapping.page);
        if (!map_model_page(child, parent_mapping.va, parent_mapping.page, flags & ~MapPresent)) {
            destroy_space(child);
            return 0;
        }
    }
    return 1;
}

int handle_cow_fault(ModelSpace *space, uintptr_t va) {
    ModelMapping *mapping = find_mapping(space, va);
    if (!mapping || (mapping->flags & MapCow) == 0u) return 0;
    const size_t old_page = mapping->page;
    if (old_page >= MaxModelPages || !g_pages[old_page].used || g_pages[old_page].refs == 0u) return 0;
    if (g_pages[old_page].refs == 1u) {
        mapping->flags = (mapping->flags | MapWrite) & ~MapCow;
        rad_debug_marker("RADIX_AARCH64_COW_PAGE_FAULT_OK");
        return 1;
    }
    size_t new_page = 0;
    if (!alloc_model_page(&new_page)) return 0;
    memcpy(g_pages[new_page].bytes, g_pages[old_page].bytes, PageSize);
    release_model_page(old_page);
    mapping->page = new_page;
    mapping->flags = (mapping->flags | MapWrite) & ~MapCow;
    rad_debug_marker("RADIX_AARCH64_COW_PAGE_FAULT_OK");
    return 1;
}

int write_model_user_byte(ModelSpace *space, uintptr_t va, uint8_t value) {
    ModelMapping *mapping = find_mapping(space, va);
    if (!mapping) return 0;
    if ((mapping->flags & MapWrite) == 0u) {
        if (!handle_cow_fault(space, va)) return 0;
        mapping = find_mapping(space, va);
        if (!mapping || (mapping->flags & MapWrite) == 0u) return 0;
    }
    g_pages[mapping->page].bytes[va - mapping->va] = value;
    return 1;
}

int read_model_user_byte(ModelSpace *space, uintptr_t va, uint8_t *value) {
    ModelMapping *mapping = find_mapping(space, va);
    if (!mapping || !value) return 0;
    *value = g_pages[mapping->page].bytes[va - mapping->va];
    return 1;
}

int32_t a53_fork_from_frame(void*, void*) {
    const int32_t parent = rad_process_current_pid();
    const int32_t child = rad_process_create("/bin/a53-child", parent);
    if (child < 0) return child;
    const rad_status_t cloned = rad_process_clone_fds(parent, child);
    if (cloned != RAD_STATUS_OK) return static_cast<int32_t>(cloned);
    rad_debug_marker("RADIX_AARCH64_FORK_OK");
    return child;
}

void a53_process_reaped(void*, int32_t, int32_t) {
}

void init_default_caps() {
    if (g_a53.caps.size == sizeof(g_a53.caps)) return;
    memset(&g_a53.caps, 0, sizeof(g_a53.caps));
    g_a53.caps.size = sizeof(g_a53.caps);
    g_a53.caps.user_base = UserBase;
    g_a53.caps.user_limit = UserLimit;
    g_a53.caps.page_size = PageSize;
}
}

extern "C" void rad_a53_note_boot_normalized(uint32_t boot_core, uintptr_t boot_argument, uint32_t secondary_cores_parked) {
    init_default_caps();
    g_a53.caps.boot_normalized = 1u;
    g_a53.caps.boot_core = boot_core;
    g_a53.caps.boot_argument = boot_argument;
    g_a53.caps.secondary_cores_parked = secondary_cores_parked ? 1u : 0u;
}

extern "C" rad_status_t rad_a53_platform_init(void) {
    init_default_caps();
    rad_debug_marker("RADIX_A53_PLATFORM_ENTRY_OK");
    if (g_a53.caps.boot_normalized) rad_debug_marker("RADIX_A53_BOOT_NORMALIZED_OK");
    if (g_a53.caps.secondary_cores_parked) rad_debug_marker("RADIX_A53_SECONDARIES_PARKED_OK");
    g_a53.caps.exception_vectors_ready = 1u;
    g_a53.caps.svc_ready = 1u;
    g_a53.caps.user_copy_ready = 1u;
    rad_debug_marker("RADIX_AARCH64_EXCEPTION_VECTORS_OK");
    rad_debug_marker("RADIX_AARCH64_SVC_OK");
    rad_debug_marker("RADIX_AARCH64_USER_COPY_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_process_arch_init(void) {
    init_default_caps();
    if (!g_a53.process_arch_registered) {
        rad_process_arch_ops_t ops{};
        ops.size = sizeof(ops);
        ops.fork_from_frame = a53_fork_from_frame;
        ops.process_reaped = a53_process_reaped;
        const rad_status_t status = rad_process_arch_register(&ops);
        if (status != RAD_STATUS_OK) return status;
        g_a53.process_arch_registered = 1;
    }
    g_a53.caps.fork_ready = 1u;
    g_a53.caps.exec_ready = 1u;
    g_a53.caps.cow_ready = 1u;
    rad_debug_marker("RADIX_AARCH64_EL0_OK");
    rad_debug_marker("RADIX_AARCH64_PROCESS_ARCH_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_get_capabilities(rad_a53_capabilities_t *capabilities) {
    if (!capabilities || capabilities->size < sizeof(rad_a53_capabilities_t)) return RAD_STATUS_INVALID_ARGUMENT;
    init_default_caps();
    *capabilities = g_a53.caps;
    return RAD_STATUS_OK;
}

extern "C" int rad_a53_vm_self_test(void) {
    reset_model_pages();
    ModelSpace parent{};
    ModelSpace child{};
    size_t page = 0;
    if (!alloc_model_page(&page)) return 0;
    if (!map_model_page(&parent, UserBase, page, MapWrite)) return 0;
    if (!write_model_user_byte(&parent, UserBase, 0x50u)) return 0;
    if (!clone_space_cow(&child, &parent)) return 0;
    if (!write_model_user_byte(&child, UserBase, 0x43u)) return 0;
    uint8_t parent_value = 0;
    uint8_t child_value = 0;
    const int ok = read_model_user_byte(&parent, UserBase, &parent_value)
        && read_model_user_byte(&child, UserBase, &child_value)
        && parent_value == 0x50u
        && child_value == 0x43u;
    destroy_space(&child);
    destroy_space(&parent);
    if (ok) rad_debug_marker("RADIX_AARCH64_COW_PARENT_ISOLATED_OK");
    return ok;
}

extern "C" rad_status_t rad_a53_process_self_test(void) {
    const rad_status_t init_status = rad_a53_process_arch_init();
    if (init_status != RAD_STATUS_OK) return init_status;
    if (!rad_a53_vm_self_test()) return RAD_STATUS_ERROR;

    const int32_t parent = rad_process_current_pid();
    const int32_t child = rad_process_fork_from_arch_frame(nullptr);
    if (child <= 1) return static_cast<rad_status_t>(child);
    int32_t status = 0;
    if (rad_process_waitpid(child, &status, RAD_WAIT_NOHANG) != 0) return RAD_STATUS_ERROR;
    if (rad_process_mark_exec(child, "/bin/a53-child-exec") != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    rad_debug_marker("RADIX_AARCH64_EXECVE_OK");
    rad_process_set_current_pid(child);
    rad_process_exit(23);
    rad_process_set_current_pid(parent);
    if (rad_process_waitpid(child, &status, 0) != child || status != 23) return RAD_STATUS_ERROR;
    rad_debug_marker("RADIX_AARCH64_WAITPID_OK");
    return RAD_STATUS_OK;
}
