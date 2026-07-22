#include "x86_vm.h"

#include <radkernel/radkernel.h>

#include <string.h>

extern "C" char __kernel_start[];
extern "C" char __kernel_end[];
extern "C" void x86_serial_write(const char *text);
extern "C" uint32_t x86_cpu_current_core(void);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint64_t PageSize = 4096;
constexpr uint64_t LargePageSize = 2u * 1024u * 1024u;
constexpr uint64_t MaxTrackedPages = 262144; // 1 GiB of early physical memory.
constexpr uint64_t UserBase = 0x3ffff000u;
constexpr uint64_t UserLimit = 0x90000000u;
constexpr uint64_t UserStackTop = 0x80000000u;
constexpr uint64_t UserStackGrowLimit = 0x78000000u;
constexpr uint64_t LowIdentityLimit = 16u * 1024u * 1024u;
constexpr uint64_t KernelIdentityLimit = 4ull * 1024ull * 1024ull * 1024ull;
constexpr size_t MaxKernelMmioRanges = 16;
constexpr uint32_t MaxVmCores = 4;
constexpr uint64_t PtePresent = 1ull << 0;
constexpr uint64_t PteWrite = 1ull << 1;
constexpr uint64_t PteUser = 1ull << 2;
constexpr uint64_t PteCacheDisable = 1ull << 4;
constexpr uint64_t PteLarge = 1ull << 7;
constexpr uint64_t PteCow = 1ull << 9;
constexpr uint64_t PteAddressMask = 0x000ffffffffff000ull;

enum PageState : uint8_t {
    PageUntracked = 0,
    PageFree = 1,
    PageReserved = 2
};

uint8_t g_page_state[MaxTrackedPages];
uint16_t g_page_refs[MaxTrackedPages];
x86_vm_summary_t g_summary{};
uint64_t g_kernel_cr3 = 0;
x86_address_space_t *g_current_space[MaxVmCores]{};
volatile int g_ap_cow_seen = 0;

struct KernelMmioRange {
    uint64_t base;
    uint64_t size;
};

KernelMmioRange g_kernel_mmio_ranges[MaxKernelMmioRanges];
size_t g_kernel_mmio_range_count = 0;

// Recursive spinlock protecting the shared physical-page tracker (g_page_state /
// g_page_refs). Without it, two cores forking / COW-faulting / mmap-ing at once could
// both claim the same free page (a TOCTOU on g_page_state) and double-allocate it, or
// race the shared refcounts -- SMP-only heap/page corruption. Recursive so the COW and
// fork paths can hold it across their own alloc/free calls.
extern "C" uint32_t x86_cpu_current_core(void);
volatile int g_page_spin = 0;
volatile uint32_t g_page_lock_owner = 0xffffffffu;
volatile int g_page_lock_depth = 0;

void page_tracker_lock(void) {
    const uint32_t core = x86_cpu_current_core();
    if (g_page_lock_depth > 0 && g_page_lock_owner == core) {
        ++g_page_lock_depth;
        return;
    }
    while (__atomic_test_and_set(&g_page_spin, __ATOMIC_ACQUIRE)) {
        asm volatile("pause");
    }
    g_page_lock_owner = core;
    g_page_lock_depth = 1;
}

void page_tracker_unlock(void) {
    if (g_page_lock_depth > 1) {
        --g_page_lock_depth;
        return;
    }
    g_page_lock_owner = 0xffffffffu;
    g_page_lock_depth = 0;
    __atomic_clear(&g_page_spin, __ATOMIC_RELEASE);
}

uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1u);
}

uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

void reserve_range(uint64_t base, uint64_t size) {
    const uint64_t begin = align_down(base, PageSize) / PageSize;
    const uint64_t end = align_up(base + size, PageSize) / PageSize;
    for (uint64_t page = begin; page < end && page < MaxTrackedPages; ++page) {
        if (g_page_state[page] == PageFree && g_summary.free_pages > 0) {
            --g_summary.free_pages;
            ++g_summary.reserved_pages;
        }
        g_page_state[page] = PageReserved;
    }
}

void mark_usable(uint64_t base, uint64_t size) {
    const uint64_t begin = align_up(base, PageSize) / PageSize;
    const uint64_t end = align_down(base + size, PageSize) / PageSize;
    for (uint64_t page = begin; page < end && page < MaxTrackedPages; ++page) {
        if (g_page_state[page] == PageUntracked) {
            g_page_state[page] = PageFree;
            ++g_summary.tracked_pages;
            ++g_summary.free_pages;
        }
    }
}

int create_user_address_space_probe(void) {
    x86_address_space_t space{};
    if (!x86_vm_create_address_space(&space)) return 0;
    const int isolated = x86_vm_address_space_isolated(&space);
    x86_vm_destroy_address_space(&space);
    return isolated;
}

uint64_t read_cr3(void) {
    uint64_t value = 0;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

void write_cr3(uint64_t value) {
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

uint16_t table_index(uint64_t virtual_address, uint8_t level) {
    return static_cast<uint16_t>((virtual_address >> (12u + level * 9u)) & 0x1ffu);
}

uint64_t *table_ptr(uint64_t physical_address) {
    return reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(physical_address));
}

x86_address_space_t *current_space(void) {
    const uint32_t core = x86_cpu_current_core();
    return core < MaxVmCores ? g_current_space[core] : g_current_space[0];
}

void set_current_space(x86_address_space_t *space) {
    const uint32_t core = x86_cpu_current_core();
    if (core < MaxVmCores) g_current_space[core] = space;
    else g_current_space[0] = space;
}

int record_owned_page(x86_address_space_t *space, uint64_t page) {
    if (!space || space->owned_page_count >= (sizeof(space->owned_pages) / sizeof(space->owned_pages[0]))) return 0;
    space->owned_pages[space->owned_page_count++] = page;
    return 1;
}

int replace_owned_page(x86_address_space_t *space, uint64_t old_page, uint64_t new_page) {
    if (!space) return 0;
    for (size_t i = 0; i < space->owned_page_count; ++i) {
        if (space->owned_pages[i] != old_page) continue;
        space->owned_pages[i] = new_page;
        return 1;
    }
    return record_owned_page(space, new_page);
}

int retain_page(uint64_t physical_address) {
    if (physical_address % PageSize) return 0;
    const uint64_t page = physical_address / PageSize;
    page_tracker_lock();
    if (page >= MaxTrackedPages || g_page_state[page] != PageReserved || g_page_refs[page] == 0) {
        page_tracker_unlock();
        return 0;
    }
    ++g_page_refs[page];
    page_tracker_unlock();
    return 1;
}

uint64_t alloc_owned_page(x86_address_space_t *space) {
    const uint64_t page = x86_vm_alloc_page();
    if (!page) return 0;
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(page)), 0, PageSize);
    if (!record_owned_page(space, page)) {
        x86_vm_free_page(page);
        return 0;
    }
    return page;
}

uint64_t *ensure_next_table(x86_address_space_t *space, uint64_t *table, uint16_t index) {
    if (!(table[index] & PtePresent)) {
        const uint64_t page = alloc_owned_page(space);
        if (!page) return nullptr;
        table[index] = page | PtePresent | PteWrite | PteUser;
    } else {
        table[index] |= PteUser;
    }
    return table_ptr(table[index] & PteAddressMask);
}

int map_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if (!space || !space->pml4 || (virtual_address % PageSize) || (physical_address % PageSize)) return 0;
    uint64_t *pml4 = table_ptr(space->pml4);
    uint64_t *pdpt = ensure_next_table(space, pml4, table_index(virtual_address, 3));
    if (!pdpt) return 0;
    uint64_t *pd = ensure_next_table(space, pdpt, table_index(virtual_address, 2));
    if (!pd) return 0;
    uint64_t *pt = ensure_next_table(space, pd, table_index(virtual_address, 1));
    if (!pt) return 0;
    pt[table_index(virtual_address, 0)] = (physical_address & PteAddressMask) | flags | PtePresent;
    return 1;
}

int map_large_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if (!space || !space->pml4 || (virtual_address % LargePageSize) || (physical_address % LargePageSize)) return 0;
    uint64_t *pml4 = table_ptr(space->pml4);
    uint64_t *pdpt = ensure_next_table(space, pml4, table_index(virtual_address, 3));
    if (!pdpt) return 0;
    uint64_t *pd = ensure_next_table(space, pdpt, table_index(virtual_address, 2));
    if (!pd) return 0;
    pd[table_index(virtual_address, 1)] = (physical_address & PteAddressMask) | flags | PtePresent | PteLarge;
    return 1;
}

int map_kernel_identity(x86_address_space_t *space) {
    for (uint64_t address = 0; address < LowIdentityLimit; address += PageSize) {
        if (address < UserLimit && address + PageSize > UserBase) continue;
        if (!map_page(space, address, address, PteWrite)) return 0;
    }
    for (uint64_t address = LowIdentityLimit; address < KernelIdentityLimit; address += LargePageSize) {
        if (address < UserLimit && address + LargePageSize > UserBase) continue;
        if (!map_large_page(space, address, address, PteWrite)) return 0;
    }
    for (size_t i = 0; i < g_kernel_mmio_range_count; ++i) {
        const uint64_t begin = g_kernel_mmio_ranges[i].base;
        const uint64_t end = begin + g_kernel_mmio_ranges[i].size;
        for (uint64_t address = begin; address < end; address += PageSize) {
            if (!map_page(space, address, address, PteWrite | PteCacheDisable)) return 0;
        }
    }
    return 1;
}

int walk_user(uint64_t virtual_address, uint64_t *entry_out) {
    x86_address_space_t *space = current_space();
    if (!space || !space->pml4) return 0;
    if (virtual_address < UserBase || virtual_address >= UserLimit) return 0;

    uint64_t *pml4 = table_ptr(space->pml4);
    uint64_t entry = pml4[table_index(virtual_address, 3)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser)) return 0;
    uint64_t *pdpt = table_ptr(entry & PteAddressMask);
    entry = pdpt[table_index(virtual_address, 2)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser)) return 0;
    if (entry & PteLarge) {
        if (entry_out) *entry_out = entry;
        return 1;
    }
    uint64_t *pd = table_ptr(entry & PteAddressMask);
    entry = pd[table_index(virtual_address, 1)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser)) return 0;
    if (entry & PteLarge) {
        if (entry_out) *entry_out = entry;
        return 1;
    }
    uint64_t *pt = table_ptr(entry & PteAddressMask);
    entry = pt[table_index(virtual_address, 0)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser)) return 0;
    if (entry_out) *entry_out = entry;
    return 1;
}

uint64_t *walk_user_leaf(x86_address_space_t *space, uint64_t virtual_address) {
    if (!space || !space->pml4 || virtual_address < UserBase || virtual_address >= UserLimit) return nullptr;
    uint64_t *pml4 = table_ptr(space->pml4);
    uint64_t entry = pml4[table_index(virtual_address, 3)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser)) return nullptr;
    uint64_t *pdpt = table_ptr(entry & PteAddressMask);
    entry = pdpt[table_index(virtual_address, 2)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser) || (entry & PteLarge)) return nullptr;
    uint64_t *pd = table_ptr(entry & PteAddressMask);
    entry = pd[table_index(virtual_address, 1)];
    if ((entry & (PtePresent | PteUser)) != (PtePresent | PteUser) || (entry & PteLarge)) return nullptr;
    uint64_t *pt = table_ptr(entry & PteAddressMask);
    uint64_t *leaf = &pt[table_index(virtual_address, 0)];
    return (*leaf & (PtePresent | PteUser)) == (PtePresent | PteUser) ? leaf : nullptr;
}

void invalidate_page(uint64_t virtual_address) {
    asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
}
}

extern "C" void x86_vm_init(const rad_boot_info_t *boot, x86_vm_summary_t *summary) {
    memset(g_page_state, 0, sizeof(g_page_state));
    memset(g_page_refs, 0, sizeof(g_page_refs));
    memset(&g_summary, 0, sizeof(g_summary));
    memset(g_current_space, 0, sizeof(g_current_space));
    g_summary.kernel_start = reinterpret_cast<uint64_t>(__kernel_start);
    g_summary.kernel_end = reinterpret_cast<uint64_t>(__kernel_end);

    if (boot) {
        for (uint32_t i = 0; i < boot->memory_region_count && i < RAD_BOOT_MAX_MEMORY_REGIONS; ++i) {
            const rad_boot_memory_region_t& region = boot->memory_regions[i];
            if (region.type != RAD_BOOT_MEMORY_USABLE) continue;
            g_summary.usable_bytes += region.size;
            mark_usable(region.base, region.size);
        }
    }

    reserve_range(0, 0x100000);
    reserve_range(g_summary.kernel_start, g_summary.kernel_end - g_summary.kernel_start);
    reserve_range(UserBase, UserLimit - UserBase);
    g_kernel_cr3 = read_cr3();
    g_summary.page_allocator_ready = g_summary.free_pages > 0 ? 1 : 0;
    g_summary.user_address_space_ready = create_user_address_space_probe();

    char line[160];
    snprintf(line, sizeof(line),
        "RAD_VM_READY usable=%llu tracked_pages=%llu free_pages=%llu kernel=0x%llx-0x%llx\n",
        static_cast<unsigned long long>(g_summary.usable_bytes),
        static_cast<unsigned long long>(g_summary.tracked_pages),
        static_cast<unsigned long long>(g_summary.free_pages),
        static_cast<unsigned long long>(g_summary.kernel_start),
        static_cast<unsigned long long>(g_summary.kernel_end));
    x86_serial_write(line);
    if (g_summary.user_address_space_ready) rad_debug_marker("RAD_USER_VM_SCAFFOLD_OK");
    if (summary) *summary = g_summary;
}

extern "C" uint64_t x86_vm_alloc_page(void) {
    page_tracker_lock();
    for (uint64_t page = 0; page < MaxTrackedPages; ++page) {
        if (g_page_state[page] != PageFree) continue;
        g_page_state[page] = PageReserved;
        g_page_refs[page] = 1;
        if (g_summary.free_pages > 0) --g_summary.free_pages;
        ++g_summary.reserved_pages;
        page_tracker_unlock();
        return page * PageSize;
    }
    page_tracker_unlock();
    return 0;
}

extern "C" uint64_t x86_vm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return x86_vm_alloc_page();
    page_tracker_lock();
    uint64_t run_start = 0;
    uint64_t run = 0;
    for (uint64_t page = 0; page < MaxTrackedPages; ++page) {
        if (g_page_state[page] != PageFree) {
            run = 0;
            continue;
        }
        if (run == 0) run_start = page;
        if (++run == count) {
            for (uint64_t p = run_start; p < run_start + count; ++p) {
                g_page_state[p] = PageReserved;
                g_page_refs[p] = 1;
                if (g_summary.free_pages > 0) --g_summary.free_pages;
                ++g_summary.reserved_pages;
            }
            page_tracker_unlock();
            return run_start * PageSize;
        }
    }
    page_tracker_unlock();
    return 0;
}

extern "C" void x86_vm_free_page(uint64_t physical_address) {
    if (physical_address % PageSize) return;
    const uint64_t page = physical_address / PageSize;
    page_tracker_lock();
    if (page >= MaxTrackedPages || g_page_state[page] != PageReserved) {
        page_tracker_unlock();
        return;
    }
    if (g_page_refs[page] > 1) {
        --g_page_refs[page];
        page_tracker_unlock();
        return;
    }
    g_page_refs[page] = 0;
    g_page_state[page] = PageFree;
    ++g_summary.free_pages;
    if (g_summary.reserved_pages > 0) --g_summary.reserved_pages;
    page_tracker_unlock();
}

extern "C" int x86_vm_retain_page(uint64_t physical_address) {
    return retain_page(physical_address);
}

extern "C" int x86_vm_create_address_space(x86_address_space_t *space) {
    if (!space) return 0;
    memset(space, 0, sizeof(*space));
    space->pml4 = alloc_owned_page(space);
    if (!space->pml4) return 0;
    if (!map_kernel_identity(space)) {
        x86_vm_destroy_address_space(space);
        return 0;
    }
    return 1;
}

extern "C" int x86_vm_map_user_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable) {
    if (!space || virtual_address < UserBase || virtual_address >= UserLimit) return 0;
    const uint64_t flags = PteUser | (writable ? PteWrite : 0);
    if (!map_page(space, align_down(virtual_address, PageSize), physical_address, flags)) return 0;
    return record_owned_page(space, physical_address);
}

extern "C" int x86_vm_map_shared_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable) {
    if (!space || virtual_address < UserBase || virtual_address >= UserLimit) return 0;
    if (!x86_vm_retain_page(physical_address)) return 0;
    const uint64_t flags = PteUser | (writable ? PteWrite : 0);
    if (!map_page(space, align_down(virtual_address, PageSize), physical_address, flags) || !record_owned_page(space, physical_address)) {
        x86_vm_free_page(physical_address);
        return 0;
    }
    return 1;
}

extern "C" int x86_vm_map_kernel_mmio(uint64_t physical_address, uint64_t size) {
    if (!g_kernel_cr3 || size == 0) return 0;
    if (physical_address + size < physical_address) return 0;

    const uint64_t begin = align_down(physical_address, PageSize);
    const uint64_t end = align_up(physical_address + size, PageSize);
    x86_address_space_t kernel{};
    kernel.pml4 = g_kernel_cr3;
    for (uint64_t address = begin; address < end; address += PageSize) {
        if (!map_page(&kernel, address, address, PteWrite | PteCacheDisable)) return 0;
        invalidate_page(address);
    }

    if (begin < KernelIdentityLimit) return 1;
    const uint64_t range_size = end - begin;
    for (size_t i = 0; i < g_kernel_mmio_range_count; ++i) {
        if (g_kernel_mmio_ranges[i].base == begin && g_kernel_mmio_ranges[i].size == range_size) return 1;
    }
    if (g_kernel_mmio_range_count >= MaxKernelMmioRanges) return 1;
    g_kernel_mmio_ranges[g_kernel_mmio_range_count++] = {begin, range_size};
    return 1;
}

extern "C" int x86_vm_clone_cow(x86_address_space_t *child, x86_address_space_t *parent) {
    if (!child || !parent || !parent->pml4) return 0;
    if (!x86_vm_create_address_space(child)) return 0;
    for (uint64_t va = UserBase; va < UserLimit; va += PageSize) {
        uint64_t *parent_leaf = walk_user_leaf(parent, va);
        if (!parent_leaf) continue;
        uint64_t entry = *parent_leaf;
        const uint64_t phys = entry & PteAddressMask;
        uint64_t flags = entry & ~PteAddressMask;
        if (flags & PteWrite) {
            flags = (flags & ~PteWrite) | PteCow;
            *parent_leaf = phys | flags;
            invalidate_page(va);
        }
        if (!retain_page(phys)) {
            x86_vm_destroy_address_space(child);
            return 0;
        }
        if (!map_page(child, va, phys, flags & ~PtePresent) || !record_owned_page(child, phys)) {
            x86_vm_free_page(phys);
            x86_vm_destroy_address_space(child);
            return 0;
        }
    }
    return 1;
}

extern "C" int x86_vm_handle_page_fault(uint64_t fault_address, uint64_t error_code) {
    x86_address_space_t *space = current_space();
    if (!space) return 0;
    const int present = static_cast<int>((error_code >> 0u) & 1u);
    const int write = static_cast<int>((error_code >> 1u) & 1u);
    const int user = static_cast<int>((error_code >> 2u) & 1u);
    const uint64_t va = align_down(fault_address, PageSize);
    if (!present && user && fault_address >= UserStackGrowLimit && fault_address < UserStackTop) {
        const uint64_t phys = x86_vm_alloc_page();
        if (!phys) return 0;
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(phys)), 0, PageSize);
        if (!map_page(space, va, phys, PteUser | PteWrite) || !record_owned_page(space, phys)) {
            x86_vm_free_page(phys);
            return 0;
        }
        invalidate_page(va);
        rad_debug_marker("RAD_USER_STACK_GROW_OK");
        return 1;
    }
    if (!write || !user) return 0;
    uint64_t *leaf = walk_user_leaf(space, va);
    if (!leaf || ((*leaf & PteCow) == 0)) return 0;
    const uint64_t old_phys = *leaf & PteAddressMask;
    const uint64_t old_page = old_phys / PageSize;
    // The shared-refcount decision and the resulting alloc/copy/free must be atomic
    // against another core COW-faulting the same shared page. The page tracker lock is
    // recursive, so the nested x86_vm_alloc_page/free_page below are safe.
    page_tracker_lock();
    if (old_page >= MaxTrackedPages || g_page_refs[old_page] == 0) {
        page_tracker_unlock();
        return 0;
    }
    if (g_page_refs[old_page] == 1) {
        *leaf = (*leaf | PteWrite) & ~PteCow;
        invalidate_page(va);
        page_tracker_unlock();
        rad_debug_marker("RAD_USER_COW_PAGE_FAULT_OK");
        if (x86_cpu_current_core() > 0 && !__atomic_exchange_n(&g_ap_cow_seen, 1, __ATOMIC_ACQ_REL)) {
            rad_debug_marker("RAD_USER_AP_FORK_COW_OK");
        }
        return 1;
    }
    const uint64_t new_phys = x86_vm_alloc_page();
    if (!new_phys) {
        page_tracker_unlock();
        return 0;
    }
    memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(new_phys)),
        reinterpret_cast<const void*>(static_cast<uintptr_t>(old_phys)),
        PageSize);
    if (!replace_owned_page(space, old_phys, new_phys)) {
        x86_vm_free_page(new_phys);
        page_tracker_unlock();
        return 0;
    }
    x86_vm_free_page(old_phys);
    *leaf = new_phys | ((*leaf | PteWrite) & ~(PteAddressMask | PteCow));
    invalidate_page(va);
    page_tracker_unlock();
    rad_debug_marker("RAD_USER_COW_PAGE_FAULT_OK");
    if (x86_cpu_current_core() > 0 && !__atomic_exchange_n(&g_ap_cow_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RAD_USER_AP_FORK_COW_OK");
    }
    return 1;
}

extern "C" int x86_page_fault_dispatch(uint64_t error_code) {
    uint64_t fault_address = 0;
    asm volatile("mov %%cr2, %0" : "=r"(fault_address));
    return x86_vm_handle_page_fault(fault_address, error_code);
}

extern "C" void x86_vm_destroy_address_space(x86_address_space_t *space) {
    if (!space) return;
    for (size_t i = space->owned_page_count; i > 0; --i) {
        x86_vm_free_page(space->owned_pages[i - 1]);
    }
    memset(space, 0, sizeof(*space));
}

extern "C" void x86_vm_activate_address_space(const x86_address_space_t *space) {
    if (!space || !space->pml4) return;
    set_current_space(const_cast<x86_address_space_t*>(space));
    write_cr3(space->pml4);
}

extern "C" void x86_vm_activate_kernel_address_space(void) {
    if (g_kernel_cr3) write_cr3(g_kernel_cr3);
    set_current_space(nullptr);
}

extern "C" int x86_vm_validate_user_range(uint64_t virtual_address, uint64_t size, int write) {
    if (size == 0) return virtual_address >= UserBase && virtual_address <= UserLimit;
    if (virtual_address < UserBase || virtual_address >= UserLimit) return 0;
    if (size > UserLimit - virtual_address) return 0;
    uint64_t address = virtual_address;
    const uint64_t end = virtual_address + size;
    while (address < end) {
        uint64_t entry = 0;
        if (!walk_user(address, &entry)) return 0;
        if (write && !(entry & PteWrite)) return 0;
        const uint64_t next = align_down(address, PageSize) + PageSize;
        address = next > address ? next : end;
    }
    return 1;
}

extern "C" int x86_vm_address_space_isolated(const x86_address_space_t *space) {
    if (!space || !space->pml4) return 0;
    x86_address_space_t *previous = current_space();
    set_current_space(const_cast<x86_address_space_t*>(space));
    const int kernel_not_user = !x86_vm_validate_user_range(g_summary.kernel_start, 1, 0);
    const int low_not_user = !x86_vm_validate_user_range(0x1000u, 1, 0);
    set_current_space(previous);
    return kernel_not_user && low_not_user;
}

extern "C" int x86_vm_self_test(void) {
    const uint64_t page = x86_vm_alloc_page();
    if (!page) {
        rad_debug_marker("RAD_VM_PAGE_ALLOC_FAIL");
        return 0;
    }
    volatile uint64_t *word = reinterpret_cast<volatile uint64_t*>(static_cast<uintptr_t>(page));
    *word = 0x5241444958504147ull; // "RADPAG"
    const int ok = *word == 0x5241444958504147ull;
    x86_vm_free_page(page);
    rad_debug_marker(ok ? "RAD_VM_PAGE_ALLOC_OK" : "RAD_VM_PAGE_ALLOC_FAIL");
    return ok;
}
