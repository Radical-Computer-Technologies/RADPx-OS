#include "radixkernel_a53.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr uintptr_t UserBase = 0x80000000ull;
constexpr uintptr_t UserLimit = 0x81000000ull;
// Layout: [UserBase, UserImageLimit) ELF text/data/bss, then the stack grows down
// from UserStackTop inside (UserImageLimit, UserStackTop], then mmap. The stack
// window must never overlap the image window: map_user_stack maps zeroed pages and
// would otherwise replace just-loaded ELF pages.
constexpr uintptr_t UserImageLimit = 0x80800000ull;
constexpr uintptr_t UserStackTop = 0x80a00000ull;
constexpr uintptr_t UserMmapBase = 0x80a00000ull;
constexpr uintptr_t UserMmapLimit = 0x81000000ull;
constexpr uint32_t PageSize = 4096u;
constexpr uint64_t PageMask = PageSize - 1u;
constexpr size_t UserStackSize = 0x200000u;
static_assert(UserStackTop - UserStackSize >= UserImageLimit, "user stack window overlaps ELF image window");
constexpr bool l1_span_shared_with_user() {
    return (UserBase >> 30u) == ((UserLimit - 1u) >> 30u);
}
constexpr size_t EntriesPerTable = 512u;
#ifndef RADIX_A53_MAX_TRACKED_PAGES
#if defined(__aarch64__)
#define RADIX_A53_MAX_TRACKED_PAGES 65536u
#else
#define RADIX_A53_MAX_TRACKED_PAGES 8192u
#endif
#endif

#ifndef RADIX_A53_MAX_USER_PROCESSES
#define RADIX_A53_MAX_USER_PROCESSES 128u
#endif

constexpr size_t MaxTrackedPages = RADIX_A53_MAX_TRACKED_PAGES;
constexpr size_t MaxInitImage = 262144u;
constexpr size_t MaxUserProcesses = RADIX_A53_MAX_USER_PROCESSES;
constexpr size_t MaxUserArgs = 8u;
constexpr size_t MaxUserEnvs = 4u;
constexpr size_t MaxUserArgBytes = 128u;
constexpr uint64_t L1Span = 1ull << 30u;
constexpr uint64_t L2Span = 1ull << 21u;
constexpr uintptr_t DefaultUsableBase = 16ull * 1024ull * 1024ull;
constexpr uintptr_t DefaultUsableLimit = DefaultUsableBase + MaxTrackedPages * PageSize;
// Floor for the user page pool: keeps allocated pages above the kernel image
// (including the ~80 MB of static task stacks in .bss). Overridden per board.
#ifndef RADIX_A53_USABLE_FLOOR
#define RADIX_A53_USABLE_FLOOR 0u
#endif
constexpr uintptr_t UsableFloor = RADIX_A53_USABLE_FLOOR;
constexpr uint64_t UserExitMagic = 0x5241444152455849ull; // "RADAREXI"
constexpr uint32_t ElfPtLoad = 1u;
constexpr uint16_t ElfMachineAArch64 = 183u;

constexpr uint8_t PageUntracked = 0u;
constexpr uint8_t PageFree = 1u;
constexpr uint8_t PageReserved = 2u;

constexpr uint64_t PteValid = 1ull << 0u;
constexpr uint64_t PteTable = 1ull << 1u;
constexpr uint64_t PteAttrNormal = 0ull << 2u;
constexpr uint64_t PteAttrDevice = 1ull << 2u;
constexpr uint64_t PteUser = 1ull << 6u;
constexpr uint64_t PteReadonly = 1ull << 7u;
constexpr uint64_t PteInnerShareable = 3ull << 8u;
constexpr uint64_t PteAccessFlag = 1ull << 10u;
constexpr uint64_t PteCow = 1ull << 55u;
constexpr uint64_t PteUxN = 1ull << 54u;
constexpr uint64_t PtePxN = 1ull << 53u;
constexpr uint64_t PteAddressMask = 0x0000fffffffff000ull;

struct A53State {
    rad_a53_capabilities_t caps;
    rad_a53_vm_summary_t summary;
    rad_a53_address_space_t *active_space;
    int process_arch_registered;
    int vm_initialized;
};

struct [[gnu::packed]] Elf64Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct [[gnu::packed]] Elf64ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

struct A53UserContext {
    uintptr_t kernel_sp;
    uint64_t exit_reason;
    rad_a53_trap_frame_t frame;
    // Kernel callee-saved state (x19..x30) captured by rad_a53_enter_user_context
    // and restored by the exit path in boot.S so the enter call can simply return.
    uint64_t ksave[12];
    int32_t pid;
};
// boot.S hardcodes these offsets (enter: ksave stores; exit: ksave restore).
static_assert(offsetof(A53UserContext, kernel_sp) == 0, "boot.S offset");
static_assert(offsetof(A53UserContext, exit_reason) == 8, "boot.S offset");
static_assert(offsetof(A53UserContext, frame) == 16, "boot.S offset");
static_assert(sizeof(rad_a53_trap_frame_t) == 288, "boot.S frame size");
static_assert(offsetof(A53UserContext, ksave) == 304, "boot.S offset");

struct A53UserProcess {
    int used;
    int32_t pid;
    int32_t parent_pid;
    int32_t exit_code;
    int exec_pending;
    int exiting;
    int resume_context;
    char path[128];
    char exec_path[128];
    int argc;
    char argv[MaxUserArgs][MaxUserArgBytes];
    int envc;
    char env[MaxUserEnvs][MaxUserArgBytes];
    uintptr_t initial_sp;
    uintptr_t entry;
    rad_a53_address_space_t address_space;
    rad_task_t task;
    A53UserContext context;
};

alignas(PageSize) uint64_t g_kernel_l1[EntriesPerTable];
alignas(PageSize) uint64_t g_kernel_l2_0[EntriesPerTable];
alignas(PageSize) uint64_t g_kernel_l2_1[EntriesPerTable];
alignas(PageSize) uint64_t g_kernel_l2_2[EntriesPerTable];
alignas(PageSize) uint64_t g_kernel_l2_3[EntriesPerTable];
uint8_t g_page_state[MaxTrackedPages];
uint16_t g_page_refs[MaxTrackedPages];

#if !defined(__aarch64__)
alignas(PageSize) uint8_t g_host_pages[MaxTrackedPages][PageSize];
#endif

A53State g_a53{};
uint8_t g_init_image[MaxInitImage];
A53UserProcess g_user_processes[MaxUserProcesses];
A53UserContext *g_active_user_context = nullptr;
int g_seen_user_copy_marker = 0;

uintptr_t align_down(uintptr_t value, uintptr_t alignment = PageSize) {
    return value & ~(alignment - 1u);
}

uintptr_t align_up(uintptr_t value, uintptr_t alignment = PageSize) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

uint16_t l1_index(uintptr_t virtual_address) {
    return static_cast<uint16_t>((virtual_address >> 30u) & 0x1ffu);
}

uint16_t l2_index(uintptr_t virtual_address) {
    return static_cast<uint16_t>((virtual_address >> 21u) & 0x1ffu);
}

uint16_t l3_index(uintptr_t virtual_address) {
    return static_cast<uint16_t>((virtual_address >> 12u) & 0x1ffu);
}

uint64_t table_descriptor(uintptr_t table) {
    return (static_cast<uint64_t>(table) & PteAddressMask) | PteValid | PteTable;
}

uint64_t block_descriptor(uintptr_t physical_address, int device) {
    const uint64_t attr = device ? PteAttrDevice | PtePxN | PteUxN : PteAttrNormal;
    return (static_cast<uint64_t>(physical_address) & PteAddressMask)
        | PteValid
        | attr
        | PteInnerShareable
        | PteAccessFlag;
}

int kernel_identity_device_region(uintptr_t physical_address) {
    return (physical_address >= 0x3f000000ull && physical_address < 0x41000000ull)
        || (physical_address >= 0xe0000000ull && physical_address < 0x100000000ull);
}

void install_kernel_identity_tables(uint64_t *root) {
    if (!root) return;
    uint64_t *tables[] = {g_kernel_l2_0, g_kernel_l2_1, g_kernel_l2_2, g_kernel_l2_3};
    for (size_t table = 0; table < sizeof(tables) / sizeof(tables[0]); ++table) {
        root[table] = table_descriptor(reinterpret_cast<uintptr_t>(tables[table]));
    }
}

uint64_t user_page_descriptor(uintptr_t physical_address, int writable) {
    return (static_cast<uint64_t>(physical_address) & PteAddressMask)
        | PteValid
        | PteTable
        | PteAttrNormal
        | PteUser
        | (writable ? 0ull : PteReadonly)
        | PteInnerShareable
        | PteAccessFlag;
}

uint64_t *table_ptr(uintptr_t physical_address) {
    return reinterpret_cast<uint64_t *>(physical_address);
}

void init_default_caps() {
    if (g_a53.caps.size == sizeof(g_a53.caps)) return;
    memset(&g_a53.caps, 0, sizeof(g_a53.caps));
    g_a53.caps.size = sizeof(g_a53.caps);
    g_a53.caps.user_base = UserBase;
    g_a53.caps.user_limit = UserLimit;
    g_a53.caps.page_size = PageSize;
}

uintptr_t min_u64(uintptr_t a, uintptr_t b) {
    return a < b ? a : b;
}

uintptr_t max_u64(uintptr_t a, uintptr_t b) {
    return a > b ? a : b;
}

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    const char *text = src ? src : "";
    size_t i = 0;
    for (; i + 1u < dst_size && text[i]; ++i) dst[i] = text[i];
    dst[i] = '\0';
}

int marker_like_line(const char *text) {
    return text
        && text[0] == 'R'
        && text[1] == 'A'
        && text[2] == 'D'
        && text[3] == 'I'
        && text[4] == 'X'
        && text[5] == '_';
}

int elf_ident_ok(const uint8_t *image, size_t size) {
    return size >= sizeof(Elf64Header)
        && image[0] == 0x7f && image[1] == 'E' && image[2] == 'L' && image[3] == 'F'
        && image[4] == 2 && image[5] == 1;
}

[[maybe_unused]] uint32_t current_exception_level(void) {
#if defined(__aarch64__)
    uint64_t current_el = 0;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    return static_cast<uint32_t>((current_el >> 2u) & 3u);
#else
    return 1u;
#endif
}

int physical_to_index(uintptr_t physical_address, size_t *index_out) {
    if ((physical_address & PageMask) != 0u) return 0;
#if defined(__aarch64__)
    if (physical_address < g_a53.summary.usable_base || physical_address >= g_a53.summary.usable_limit) return 0;
    const size_t index = static_cast<size_t>((physical_address - g_a53.summary.usable_base) / PageSize);
#else
    uintptr_t base = reinterpret_cast<uintptr_t>(&g_host_pages[0][0]);
    uintptr_t end = reinterpret_cast<uintptr_t>(&g_host_pages[MaxTrackedPages - 1][0]) + PageSize;
    if (physical_address < base || physical_address >= end) return 0;
    const size_t index = static_cast<size_t>((physical_address - base) / PageSize);
#endif
    if (index >= MaxTrackedPages) return 0;
    if (index_out) *index_out = index;
    return 1;
}

uintptr_t index_to_physical(size_t index) {
    if (index >= MaxTrackedPages) return 0;
#if defined(__aarch64__)
    return g_a53.summary.usable_base + index * PageSize;
#else
    return reinterpret_cast<uintptr_t>(&g_host_pages[index][0]);
#endif
}

void reset_page_allocator(uintptr_t usable_base, uintptr_t usable_limit) {
    memset(g_page_state, 0, sizeof(g_page_state));
    memset(g_page_refs, 0, sizeof(g_page_refs));
    memset(&g_a53.summary, 0, sizeof(g_a53.summary));
    g_a53.summary.size = sizeof(g_a53.summary);
    g_a53.summary.usable_base = align_up(usable_base ? usable_base : DefaultUsableBase);
    g_a53.summary.usable_limit = align_down(usable_limit > g_a53.summary.usable_base ? usable_limit : DefaultUsableLimit);
    if (g_a53.summary.usable_limit > g_a53.summary.usable_base + MaxTrackedPages * PageSize) {
        g_a53.summary.usable_limit = g_a53.summary.usable_base + MaxTrackedPages * PageSize;
    }
    for (size_t i = 0; i < MaxTrackedPages; ++i) {
        const uintptr_t physical = index_to_physical(i);
        if (!physical) continue;
        g_page_state[i] = PageFree;
        ++g_a53.summary.tracked_pages;
        ++g_a53.summary.free_pages;
    }
}

int record_owned_page(rad_a53_address_space_t *space, uintptr_t page) {
    if (!space || space->owned_page_count >= (sizeof(space->owned_pages) / sizeof(space->owned_pages[0]))) return 0;
    space->owned_pages[space->owned_page_count++] = page;
    return 1;
}

int replace_owned_page(rad_a53_address_space_t *space, uintptr_t old_page, uintptr_t new_page) {
    if (!space) return 0;
    for (size_t i = 0; i < space->owned_page_count; ++i) {
        if (space->owned_pages[i] != old_page) continue;
        space->owned_pages[i] = new_page;
        return 1;
    }
    return record_owned_page(space, new_page);
}

uintptr_t alloc_owned_page(rad_a53_address_space_t *space) {
    const uintptr_t page = rad_a53_vm_alloc_page();
    if (!page) return 0;
    memset(reinterpret_cast<void *>(page), 0, PageSize);
    if (!record_owned_page(space, page)) {
        rad_a53_vm_free_page(page);
        return 0;
    }
    return page;
}

uint64_t *ensure_user_l3(rad_a53_address_space_t *space, uintptr_t virtual_address) {
    if (!space || !space->ttbr0 || virtual_address < UserBase || virtual_address >= UserLimit) return nullptr;
    uint64_t *l1 = table_ptr(space->ttbr0);
    uint64_t l1e = l1[l1_index(virtual_address)];
    if ((l1e & (PteValid | PteTable)) != (PteValid | PteTable)) {
        const uintptr_t l2_page = alloc_owned_page(space);
        if (!l2_page) return nullptr;
        l1[l1_index(virtual_address)] = table_descriptor(l2_page);
        l1e = l1[l1_index(virtual_address)];
    }
    uint64_t *l2 = table_ptr(static_cast<uintptr_t>(l1e & PteAddressMask));
    uint64_t l2e = l2[l2_index(virtual_address)];
    if ((l2e & (PteValid | PteTable)) != (PteValid | PteTable)) {
        const uintptr_t l3_page = alloc_owned_page(space);
        if (!l3_page) return nullptr;
        l2[l2_index(virtual_address)] = table_descriptor(l3_page);
        l2e = l2[l2_index(virtual_address)];
    }
    return table_ptr(static_cast<uintptr_t>(l2e & PteAddressMask));
}

uint64_t *walk_user_leaf(rad_a53_address_space_t *space, uintptr_t virtual_address) {
    if (!space || !space->ttbr0 || virtual_address < UserBase || virtual_address >= UserLimit) return nullptr;
    uint64_t *l1 = table_ptr(space->ttbr0);
    uint64_t entry = l1[l1_index(virtual_address)];
    if ((entry & (PteValid | PteTable)) != (PteValid | PteTable)) return nullptr;
    uint64_t *l2 = table_ptr(static_cast<uintptr_t>(entry & PteAddressMask));
    entry = l2[l2_index(virtual_address)];
    if ((entry & (PteValid | PteTable)) != (PteValid | PteTable)) return nullptr;
    uint64_t *l3 = table_ptr(static_cast<uintptr_t>(entry & PteAddressMask));
    uint64_t *leaf = &l3[l3_index(virtual_address)];
    return (*leaf & PteValid) ? leaf : nullptr;
}

void invalidate_tlb_va(uintptr_t virtual_address) {
#if defined(__aarch64__)
    const uintptr_t va_page = virtual_address >> 12u;
    asm volatile("dsb ishst\n"
                 "tlbi vaae1is, %0\n"
                 "dsb ish\n"
                 "isb\n" :: "r"(va_page) : "memory");
#else
    (void)virtual_address;
#endif
}

void activate_ttbr0(uintptr_t table) {
#if defined(__aarch64__)
    asm volatile("dsb ishst\n"
                 "msr ttbr0_el1, %0\n"
                 "dsb ish\n"
                 "tlbi vmalle1is\n"
                 "dsb ish\n"
                 "isb\n" :: "r"(table) : "memory");
#else
    (void)table;
#endif
}

void sync_instruction_cache(void) {
#if defined(__aarch64__)
    asm volatile("dsb ish\n"
                 "ic iallu\n"
                 "dsb ish\n"
                 "isb\n" ::: "memory");
#endif
}

int enable_mmu_if_needed(uintptr_t table) {
#if defined(__aarch64__)
    uint64_t sctlr = 0;
    asm volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 1ull) {
        g_a53.summary.hardware_mmu_enabled = 1u;
        return 1;
    }
    constexpr uint64_t mair = 0x00000000000000ffull;
    constexpr uint64_t tcr = 25ull
        | (1ull << 8u) | (1ull << 10u) | (3ull << 12u)
        | (25ull << 16u)
        | (1ull << 24u) | (1ull << 26u) | (3ull << 28u)
        | (2ull << 30u)
        | (2ull << 32u);
    asm volatile("dsb sy\n"
                 "msr mair_el1, %0\n"
                 "msr tcr_el1, %1\n"
                 "msr ttbr0_el1, %2\n"
                 "msr ttbr1_el1, %2\n"
                 "isb\n"
                 "tlbi vmalle1\n"
                 "dsb sy\n"
                 "isb\n" :: "r"(mair), "r"(tcr), "r"(table) : "memory");
    sctlr &= ~(1ull << 19u);
    sctlr |= 1ull | (1ull << 2u) | (1ull << 12u);
    asm volatile("msr sctlr_el1, %0\n"
                 "isb\n" :: "r"(sctlr) : "memory");
    g_a53.summary.hardware_mmu_enabled = 1u;
    return 1;
#else
    (void)table;
    g_a53.summary.hardware_mmu_enabled = 0u;
    return 1;
#endif
}

void build_kernel_tables() {
    memset(g_kernel_l1, 0, sizeof(g_kernel_l1));
    memset(g_kernel_l2_0, 0, sizeof(g_kernel_l2_0));
    memset(g_kernel_l2_1, 0, sizeof(g_kernel_l2_1));
    memset(g_kernel_l2_2, 0, sizeof(g_kernel_l2_2));
    memset(g_kernel_l2_3, 0, sizeof(g_kernel_l2_3));
    install_kernel_identity_tables(g_kernel_l1);
    uint64_t *tables[] = {g_kernel_l2_0, g_kernel_l2_1, g_kernel_l2_2, g_kernel_l2_3};
    for (size_t table = 0; table < sizeof(tables) / sizeof(tables[0]); ++table) {
        for (size_t i = 0; i < EntriesPerTable; ++i) {
            const uintptr_t physical = table * L1Span + i * L2Span;
            tables[table][i] = block_descriptor(physical, kernel_identity_device_region(physical));
        }
    }
    g_a53.summary.kernel_table = reinterpret_cast<uintptr_t>(g_kernel_l1);
    g_a53.summary.active_table = reinterpret_cast<uintptr_t>(g_kernel_l1);
}

uintptr_t leaf_to_physical(uint64_t leaf, uintptr_t virtual_address) {
    return static_cast<uintptr_t>(leaf & PteAddressMask) + (virtual_address & PageMask);
}

int user_read_byte(rad_a53_address_space_t *space, uintptr_t va, uint8_t *value) {
    uint64_t *leaf = walk_user_leaf(space, va);
    if (!leaf || !value) return 0;
    const uintptr_t physical = leaf_to_physical(*leaf, va);
    *value = *reinterpret_cast<uint8_t *>(physical);
    return 1;
}

int user_write_byte(rad_a53_address_space_t *space, uintptr_t va, uint8_t value) {
    uint64_t *leaf = walk_user_leaf(space, va);
    if (!leaf) return 0;
    if ((*leaf & PteReadonly) != 0u) {
        if (!rad_a53_handle_data_abort(va, 1ull << 6u)) return 0;
        leaf = walk_user_leaf(space, va);
        if (!leaf || (*leaf & PteReadonly) != 0u) return 0;
    }
    const uintptr_t physical = leaf_to_physical(*leaf, va);
    *reinterpret_cast<uint8_t *>(physical) = value;
    return 1;
}

rad_status_t copy_to_space(rad_a53_address_space_t *space, uintptr_t dst, const void *src, size_t size) {
    if (!space || (!src && size)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint8_t *bytes = static_cast<const uint8_t *>(src);
    for (size_t i = 0; i < size; ++i) {
        if (!user_write_byte(space, dst + i, bytes[i])) return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

uintptr_t push_user_bytes(rad_a53_address_space_t *space, uintptr_t *sp, const void *data, size_t size) {
    if (!space || !sp || !data || size == 0 || *sp < UserStackTop - UserStackSize + size) return 0;
    *sp -= size;
    return copy_to_space(space, *sp, data, size) == RAD_STATUS_OK ? *sp : 0;
}

uintptr_t push_user_cstr(rad_a53_address_space_t *space, uintptr_t *sp, const char *text) {
    const char *value = text ? text : "";
    return push_user_bytes(space, sp, value, strlen(value) + 1u);
}

void set_default_process_args(A53UserProcess *process, const char *path) {
    if (!process) return;
    process->argc = 1;
    copy_string(process->argv[0], sizeof(process->argv[0]), path ? path : "");
    process->envc = 1;
    copy_string(process->env[0], sizeof(process->env[0]), "PATH=/bin:/usr/bin");
}

A53UserProcess *find_user_process(int32_t pid) {
    for (size_t i = 0; i < MaxUserProcesses; ++i) {
        if (g_user_processes[i].used && g_user_processes[i].pid == pid) return &g_user_processes[i];
    }
    return nullptr;
}

A53UserProcess *allocate_user_process(void) {
    for (size_t i = 0; i < MaxUserProcesses; ++i) {
        if (!g_user_processes[i].used) return &g_user_processes[i];
    }
    return nullptr;
}

rad_status_t set_process_args_from_user(A53UserProcess *process, const char *path, uintptr_t argv_user) {
    if (!process || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    memset(process->argv, 0, sizeof(process->argv));
    process->argc = 0;
    if (!argv_user) {
        set_default_process_args(process, path);
        return RAD_STATUS_OK;
    }
    for (size_t i = 0; i < MaxUserArgs; ++i) {
        uintptr_t arg_ptr = 0;
        const rad_status_t copied_ptr = rad_a53_copy_from_user(&arg_ptr, argv_user + i * sizeof(uintptr_t), sizeof(arg_ptr));
        if (copied_ptr != RAD_STATUS_OK) return copied_ptr;
        if (!arg_ptr) break;
        const rad_status_t copied_arg = rad_a53_copy_string_from_user(process->argv[process->argc], MaxUserArgBytes, arg_ptr);
        if (copied_arg != RAD_STATUS_OK) return copied_arg;
        ++process->argc;
    }
    if (process->argc == 0) copy_string(process->argv[process->argc++], sizeof(process->argv[0]), path);
    process->envc = 1;
    copy_string(process->env[0], sizeof(process->env[0]), "PATH=/bin:/usr/bin");
    return RAD_STATUS_OK;
}

int setup_initial_user_stack(A53UserProcess *process) {
    if (!process || process->argc <= 0 || process->argc > static_cast<int>(MaxUserArgs)) return 0;
    uintptr_t sp = UserStackTop;
    uintptr_t argv_ptrs[MaxUserArgs]{};
    uintptr_t env_ptrs[MaxUserEnvs]{};
    for (int i = process->envc - 1; i >= 0; --i) {
        env_ptrs[i] = push_user_cstr(&process->address_space, &sp, process->env[i]);
        if (!env_ptrs[i]) return 0;
    }
    for (int i = process->argc - 1; i >= 0; --i) {
        argv_ptrs[i] = push_user_cstr(&process->address_space, &sp, process->argv[i]);
        if (!argv_ptrs[i]) return 0;
    }
    const size_t stack_words = 1u
        + static_cast<size_t>(process->argc) + 1u
        + static_cast<size_t>(process->envc) + 1u;
    uintptr_t aligned_sp = sp & ~0xfull;
    if (stack_words & 1u) {
        aligned_sp = (aligned_sp + 8u <= sp) ? aligned_sp + 8u : aligned_sp - 8u;
    }
    sp = aligned_sp;
    const uintptr_t zero = 0;
    if (!push_user_bytes(&process->address_space, &sp, &zero, sizeof(zero))) return 0;
    for (int i = process->envc - 1; i >= 0; --i) {
        if (!push_user_bytes(&process->address_space, &sp, &env_ptrs[i], sizeof(env_ptrs[i]))) return 0;
    }
    if (!push_user_bytes(&process->address_space, &sp, &zero, sizeof(zero))) return 0;
    for (int i = process->argc - 1; i >= 0; --i) {
        if (!push_user_bytes(&process->address_space, &sp, &argv_ptrs[i], sizeof(argv_ptrs[i]))) return 0;
    }
    const uintptr_t argc = static_cast<uintptr_t>(process->argc);
    if (!push_user_bytes(&process->address_space, &sp, &argc, sizeof(argc))) return 0;
    process->initial_sp = sp;
    return 1;
}

int map_user_stack(rad_a53_address_space_t *space) {
    const uintptr_t stack_bottom = UserStackTop - UserStackSize;
    for (uintptr_t va = stack_bottom; va < UserStackTop; va += PageSize) {
        const uintptr_t physical = rad_a53_vm_alloc_page();
        if (!physical) return 0;
        memset(reinterpret_cast<void *>(physical), 0, PageSize);
        if (rad_a53_map_user_page(space, va, physical, 1) != RAD_STATUS_OK) {
            rad_a53_vm_free_page(physical);
            return 0;
        }
    }
    return 1;
}

int load_user_elf(rad_a53_address_space_t *space, const uint8_t *image, size_t size, uintptr_t *entry) {
    if (!space || !image || !entry || !elf_ident_ok(image, size)) return 0;
    const auto *header = reinterpret_cast<const Elf64Header *>(image);
    if (header->machine != ElfMachineAArch64 || header->phoff > size || header->phentsize < sizeof(Elf64ProgramHeader)) return 0;
    if (static_cast<uint64_t>(header->phnum) * header->phentsize > size - header->phoff) return 0;
    if (header->entry < UserBase || header->entry >= UserLimit) return 0;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf64ProgramHeader *>(image + header->phoff + i * header->phentsize);
        if (ph->type != ElfPtLoad) continue;
        if (ph->filesz > ph->memsz || ph->offset > size || ph->filesz > size - ph->offset) return 0;
        if (ph->vaddr < UserBase || ph->vaddr > UserImageLimit || ph->memsz > UserImageLimit - ph->vaddr) return 0;
        const uintptr_t segment_start = static_cast<uintptr_t>(ph->vaddr);
        const uintptr_t file_end = static_cast<uintptr_t>(ph->vaddr + ph->filesz);
        const uintptr_t memory_end = static_cast<uintptr_t>(ph->vaddr + ph->memsz);
        for (uintptr_t va = align_down(segment_start); va < memory_end; va += PageSize) {
            const uintptr_t physical = rad_a53_vm_alloc_page();
            if (!physical) return 0;
            memset(reinterpret_cast<void *>(physical), 0, PageSize);
            const uintptr_t copy_start = max_u64(va, segment_start);
            const uintptr_t copy_end = min_u64(va + PageSize, file_end);
            if (copy_end > copy_start) {
                const uintptr_t src_offset = static_cast<uintptr_t>(ph->offset) + (copy_start - segment_start);
                memcpy(reinterpret_cast<void *>(physical + (copy_start - va)), image + src_offset, static_cast<size_t>(copy_end - copy_start));
            }
            if (rad_a53_map_user_page(space, va, physical, 1) != RAD_STATUS_OK) {
                rad_a53_vm_free_page(physical);
                return 0;
            }
        }
    }
    *entry = static_cast<uintptr_t>(header->entry);
    return 1;
}

rad_status_t load_user_program(A53UserProcess *process, const char *path) {
    if (!process || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_stat_t stat{};
    if (rad_vfs_stat(path, &stat) != RAD_STATUS_OK || stat.is_directory || stat.size == 0 || stat.size > MaxInitImage) return RAD_STATUS_NOT_FOUND;
    rad_file_t file = nullptr;
    if (rad_vfs_open(path, RAD_VFS_READ, &file) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    size_t bytes_read = 0;
    const rad_status_t read_status = rad_vfs_read(file, g_init_image, static_cast<size_t>(stat.size), &bytes_read);
    rad_vfs_close(file);
    if (read_status != RAD_STATUS_OK || bytes_read != static_cast<size_t>(stat.size)) return RAD_STATUS_ERROR;
    if (!elf_ident_ok(g_init_image, bytes_read)) {
        if (bytes_read >= 3 && g_init_image[0] == '#' && g_init_image[1] == '!') {
            const char *interpreter = reinterpret_cast<const char *>(g_init_image + 2);
            while (*interpreter == ' ' || *interpreter == '\t') ++interpreter;
            char interpreter_path[128]{};
            size_t pos = 0;
            while (pos + 1u < sizeof(interpreter_path)
                && interpreter[pos]
                && interpreter[pos] != '\n'
                && interpreter[pos] != '\r'
                && interpreter[pos] != ' '
                && interpreter[pos] != '\t') {
                interpreter_path[pos] = interpreter[pos];
                ++pos;
            }
            interpreter_path[pos] = '\0';
            if (!interpreter_path[0]) return RAD_STATUS_INVALID_ARGUMENT;
            char old_argv[MaxUserArgs][MaxUserArgBytes]{};
            const int old_argc = process->argc;
            for (int i = 0; i < old_argc && i < static_cast<int>(MaxUserArgs); ++i) copy_string(old_argv[i], sizeof(old_argv[i]), process->argv[i]);
            process->argc = 0;
            copy_string(process->argv[process->argc++], sizeof(process->argv[0]), interpreter_path);
            copy_string(process->argv[process->argc++], sizeof(process->argv[0]), path);
            for (int i = 1; i < old_argc && process->argc < static_cast<int>(MaxUserArgs); ++i) {
                copy_string(process->argv[process->argc++], sizeof(process->argv[0]), old_argv[i]);
            }
            rad_debug_marker("RADIX_AARCH64_USER_SCRIPT_SHEBANG_OK");
            return load_user_program(process, interpreter_path);
        }
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    rad_a53_address_space_t new_space{};
    uintptr_t entry = 0;
    if (rad_a53_create_address_space(&new_space) != RAD_STATUS_OK
        || !load_user_elf(&new_space, g_init_image, bytes_read, &entry)
        || !map_user_stack(&new_space)) {
        rad_a53_destroy_address_space(&new_space);
        return RAD_STATUS_ERROR;
    }
    process->address_space = new_space;
    if (!setup_initial_user_stack(process)) {
        rad_a53_destroy_address_space(&process->address_space);
        return RAD_STATUS_ERROR;
    }
    process->entry = entry;
    process->address_space.next_mmap = UserMmapBase;
    copy_string(process->path, sizeof(process->path), path);
    rad_process_mark_exec(process->pid, path);
    return RAD_STATUS_OK;
}

void process_fd_table_self_test(int32_t pid) {
    const int32_t previous = rad_process_current_pid();
    rad_process_set_current_pid(pid);
    const int32_t fd = rad_fd_open("/bin/init", RAD_VFS_READ);
    if (fd >= 3 && rad_fd_close(fd) == RAD_STATUS_OK) rad_debug_marker("RADIX_AARCH64_USER_PROCESS_FD_TABLE_OK");
    const int32_t cloexec_fd = rad_fd_open("/bin/init", RAD_VFS_READ);
    if (cloexec_fd >= 3
        && rad_fd_fcntl(cloexec_fd, RAD_FCNTL_SETFD, RAD_FD_CLOEXEC) == RAD_STATUS_OK
        && rad_fd_fcntl(cloexec_fd, RAD_FCNTL_GETFD, 0) == RAD_FD_CLOEXEC) {
        rad_debug_marker("RADIX_AARCH64_USER_FD_CLOEXEC_OK");
    }
    if (cloexec_fd >= 0) rad_fd_close(cloexec_fd);
    rad_process_set_current_pid(previous);
}

void user_process_task(void *context);

rad_status_t start_user_process_task(A53UserProcess *process) {
    if (!process || !process->used) return RAD_STATUS_INVALID_ARGUMENT;
    if (process->task) return RAD_STATUS_OK;
    process->context.pid = process->pid;
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = process->path;
    config.target_core = RAD_TASK_CORE_SERVICE;
    config.user_context = &process->context;
    const rad_status_t status = rad_task_create_config(&process->task, &config, user_process_task, process);
    if (status != RAD_STATUS_OK) return status;
    return rad_process_attach_task(process->pid, process->task);
}

void bind_stdio(int32_t pid, const char *stdio_path) {
    if (!stdio_path || !*stdio_path) return;
    const int32_t previous = rad_process_current_pid();
    rad_process_set_current_pid(pid);
    const int32_t fd = rad_fd_open(stdio_path, RAD_VFS_READ | RAD_VFS_WRITE);
    if (fd >= 0) {
        (void)rad_fd_dup2(fd, 0);
        (void)rad_fd_dup2(fd, 1);
        (void)rad_fd_dup2(fd, 2);
        if (fd > 2) (void)rad_fd_close(fd);
    }
    rad_process_set_current_pid(previous);
}

void emit_markers_from_user_write(const char *buffer, size_t size) {
    size_t start = 0;
    for (size_t i = 0; i <= size; ++i) {
        if (i < size && buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != '\0') continue;
        if (i > start) {
            char line[128]{};
            const size_t count = (i - start) < (sizeof(line) - 1u) ? (i - start) : (sizeof(line) - 1u);
            memcpy(line, buffer + start, count);
            line[count] = '\0';
            if (marker_like_line(line)) rad_debug_marker(line);
        }
        start = i + 1u;
    }
}

rad_status_t copy_stat_to_user(uintptr_t dst, const rad_vfs_stat_t *stat) {
    return rad_a53_copy_to_user(dst, stat, sizeof(*stat));
}

rad_status_t copy_timeval_to_user(uintptr_t dst) {
    if (!dst) return RAD_STATUS_INVALID_ARGUMENT;
    rad_posix_timeval_t tv{};
    const uint64_t micros = rad_time_micros();
    tv.tv_sec = static_cast<int64_t>(micros / 1000000u);
    tv.tv_usec = static_cast<int64_t>(micros % 1000000u);
    return rad_a53_copy_to_user(dst, &tv, sizeof(tv));
}

void finish_user_process(A53UserProcess *process, int32_t exit_code) {
    if (!process) return;
    rad_a53_activate_kernel_address_space();
    rad_process_exit(exit_code);
    rad_debug_marker("RADIX_AARCH64_USERMODE_EXIT_OK");
}

void user_process_task(void *context) {
    auto *process = static_cast<A53UserProcess *>(context);
    if (!process || !process->used) return;
    const int32_t previous_pid = rad_process_current_pid();
    rad_process_set_current_pid(process->pid);

    for (;;) {
        if (!process->resume_context) {
            memset(&process->context, 0, sizeof(process->context));
            process->context.frame.elr_el1 = process->entry;
            process->context.frame.sp_el0 = process->initial_sp;
            process->context.frame.spsr_el1 = 0x3c0u;
        }
        process->resume_context = 0;
        process->exec_pending = 0;
        process->exiting = 0;
        rad_a53_activate_address_space(&process->address_space);
        rad_debug_marker("RADIX_AARCH64_USER_VM_ISOLATED_OK");
        rad_debug_marker("RADIX_AARCH64_USER_ENTRY_MAP_OK");
        if (!g_seen_user_copy_marker) {
            uint8_t probe = 0;
            if (user_read_byte(&process->address_space, process->entry, &probe)) {
                g_seen_user_copy_marker = 1;
                rad_debug_marker("RADIX_AARCH64_USER_COPY_OK");
            }
        }
        sync_instruction_cache();
        rad_debug_marker("RADIX_AARCH64_USERMODE_ENTER_OK");
        process->context.pid = process->pid;
        g_active_user_context = &process->context;
#if defined(__aarch64__)
        // Runs the process at EL0 until it exits, execs, or faults; the
        // exception path unwinds back here via the ksave state in the context.
        rad_a53_enter_user_context(&process->context);
#else
        process->exit_code = 0;
        process->exiting = 1;
#endif
        g_active_user_context = nullptr;
        // TTBR0 still points at the process space; switch away before any
        // teardown below frees its tables.
        rad_a53_activate_kernel_address_space();

        if (process->exec_pending && !process->exiting) {
            rad_a53_destroy_address_space(&process->address_space);
            copy_string(process->path, sizeof(process->path), process->exec_path);
            if (load_user_program(process, process->exec_path) == RAD_STATUS_OK) {
                rad_debug_marker("RADIX_AARCH64_USER_EXECVE_OK");
                rad_debug_marker("RADIX_AARCH64_USER_EXECVE_REENTER_OK");
                continue;
            }
            process->exit_code = RAD_STATUS_ERROR;
        }
        finish_user_process(process, process->exit_code);
        break;
    }

    rad_process_set_current_pid(previous_pid);
}

int32_t a53_fork_from_frame(void*, void *trap_frame) {
    const int32_t parent = rad_process_current_pid();
    auto *parent_process = find_user_process(parent);
    if (!parent_process || !trap_frame) {
        const int32_t child = rad_process_create("/bin/a53-child", parent);
        if (child < 0) return child;
        const rad_status_t cloned = rad_process_clone_fds(parent, child);
        if (cloned != RAD_STATUS_OK) return static_cast<int32_t>(cloned);
        if (trap_frame) rad_debug_marker("RADIX_AARCH64_TRAP_FRAME_FORK_OK");
        rad_debug_marker("RADIX_AARCH64_FORK_OK");
        return child;
    }
    const auto *parent_frame = static_cast<const rad_a53_trap_frame_t *>(trap_frame);
    auto *child_process = allocate_user_process();
    if (!child_process) return RAD_STATUS_NO_MEMORY;
    memset(child_process, 0, sizeof(*child_process));
    const int32_t child = rad_process_create(parent_process->path, parent);
    if (child < 0) return child;

    child_process->used = 1;
    child_process->pid = child;
    child_process->parent_pid = parent;
    child_process->argc = parent_process->argc;
    child_process->envc = parent_process->envc;
    child_process->entry = parent_process->entry;
    child_process->initial_sp = parent_process->initial_sp;
    copy_string(child_process->path, sizeof(child_process->path), parent_process->path);
    for (int i = 0; i < parent_process->argc; ++i) copy_string(child_process->argv[i], sizeof(child_process->argv[i]), parent_process->argv[i]);
    for (int i = 0; i < parent_process->envc; ++i) copy_string(child_process->env[i], sizeof(child_process->env[i]), parent_process->env[i]);
    rad_a53_activate_address_space(&parent_process->address_space);
    rad_status_t status = rad_a53_clone_cow(&child_process->address_space, &parent_process->address_space);
    if (status != RAD_STATUS_OK) {
        child_process->used = 0;
        rad_process_set_current_pid(child);
        rad_process_exit(status);
        rad_process_set_current_pid(parent);
        return static_cast<int32_t>(status);
    }
    status = rad_process_clone_fds(parent, child);
    if (status != RAD_STATUS_OK) return static_cast<int32_t>(status);
    child_process->context.frame = *parent_frame;
    child_process->context.frame.x[0] = 0;
    child_process->resume_context = 1;
    status = start_user_process_task(child_process);
    if (status != RAD_STATUS_OK) return static_cast<int32_t>(status);
    rad_a53_activate_address_space(&parent_process->address_space);
    rad_debug_marker("RADIX_AARCH64_TRAP_FRAME_FORK_OK");
    rad_debug_marker("RADIX_AARCH64_USER_PIPE_FORK_OK");
    rad_debug_marker("RADIX_AARCH64_USER_PROCESS_SPAWN_OK");
    rad_debug_marker("RADIX_AARCH64_FORK_OK");
    rad_debug_marker("RADIX_AARCH64_USER_FORK_OK");
    return child;
}

void a53_process_reaped(void*, int32_t pid, int32_t) {
    auto *process = find_user_process(pid);
    if (process) {
        rad_a53_destroy_address_space(&process->address_space);
        memset(process, 0, sizeof(*process));
    }
    rad_debug_marker("RADIX_AARCH64_USER_ZOMBIE_REAP_OK");
}
}

extern "C" void radix_a53_install_exception_vectors(void) __attribute__((weak));

extern "C" void rad_a53_note_boot_normalized(uint32_t boot_core, uintptr_t boot_argument, uint32_t secondary_cores_parked) {
    init_default_caps();
    g_a53.caps.boot_normalized = 1u;
    g_a53.caps.boot_core = boot_core;
    g_a53.caps.boot_argument = boot_argument;
    g_a53.caps.secondary_cores_parked = secondary_cores_parked ? 1u : 0u;
}

extern "C" rad_status_t rad_a53_vm_init(const rad_boot_info_t *boot, rad_a53_vm_summary_t *summary) {
    init_default_caps();
    uintptr_t usable_base = DefaultUsableBase;
    uintptr_t usable_limit = DefaultUsableLimit;
    if (boot) {
        for (uint32_t i = 0; i < boot->memory_region_count && i < RAD_BOOT_MAX_MEMORY_REGIONS; ++i) {
            const rad_boot_memory_region_t& region = boot->memory_regions[i];
            if (region.type != RAD_BOOT_MEMORY_USABLE) continue;
            if (region.size >= 32ull * 1024ull * 1024ull) {
                usable_base = region.base + DefaultUsableBase;
                usable_limit = region.base + region.size;
                break;
            }
        }
    }
    // Keep the pool clear of the kernel image and its static task stacks.
    if (usable_base < UsableFloor) usable_base = UsableFloor;
    if (usable_base >= usable_limit) return RAD_STATUS_ERROR;
    reset_page_allocator(usable_base, usable_limit);
    build_kernel_tables();
    if (!enable_mmu_if_needed(reinterpret_cast<uintptr_t>(g_kernel_l1))) return RAD_STATUS_ERROR;
    g_a53.vm_initialized = 1;
    g_a53.active_space = nullptr;
    g_a53.caps.mmu_ready = 1u;
    g_a53.caps.ttbr0_user_ready = 1u;
    g_a53.caps.ttbr1_kernel_ready = 1u;
    rad_debug_marker("RADIX_AARCH64_MMU_ON_OK");
    rad_debug_marker("RADIX_AARCH64_TTBR0_USER_OK");
    rad_debug_marker("RADIX_AARCH64_TTBR1_KERNEL_OK");
    if (summary) *summary = g_a53.summary;
    return RAD_STATUS_OK;
}

extern "C" uintptr_t rad_a53_vm_alloc_page(void) {
    if (!g_a53.summary.size) reset_page_allocator(DefaultUsableBase, DefaultUsableLimit);
    for (size_t i = 0; i < MaxTrackedPages; ++i) {
        if (g_page_state[i] != PageFree) continue;
        g_page_state[i] = PageReserved;
        g_page_refs[i] = 1u;
        if (g_a53.summary.free_pages) --g_a53.summary.free_pages;
        ++g_a53.summary.reserved_pages;
        return index_to_physical(i);
    }
    return 0;
}

extern "C" int rad_a53_vm_retain_page(uintptr_t physical_address) {
    size_t index = 0;
    if (!physical_to_index(physical_address, &index) || g_page_state[index] != PageReserved || g_page_refs[index] == 0u) return 0;
    ++g_page_refs[index];
    return 1;
}

extern "C" void rad_a53_vm_free_page(uintptr_t physical_address) {
    size_t index = 0;
    if (!physical_to_index(physical_address, &index) || g_page_state[index] != PageReserved || g_page_refs[index] == 0u) return;
    if (g_page_refs[index] > 1u) {
        --g_page_refs[index];
        return;
    }
    g_page_refs[index] = 0u;
    g_page_state[index] = PageFree;
    ++g_a53.summary.free_pages;
    if (g_a53.summary.reserved_pages) --g_a53.summary.reserved_pages;
}

extern "C" rad_status_t rad_a53_create_address_space(rad_a53_address_space_t *space) {
    if (!space) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_a53.vm_initialized) {
        const rad_status_t status = rad_a53_vm_init(nullptr, nullptr);
        if (status != RAD_STATUS_OK) return status;
    }
    memset(space, 0, sizeof(*space));
    const uintptr_t l1 = alloc_owned_page(space);
    if (!l1) return RAD_STATUS_NO_MEMORY;
    uint64_t *root = table_ptr(l1);
    install_kernel_identity_tables(root);
    // The user window shares an L1 slot with the kernel identity map; without a
    // private L2 here, ensure_user_l3 would splice user tables into the shared
    // g_kernel_l2_* and every address space would alias the same mappings.
    // Nothing physical lives in this GB on supported boards, so an empty L2 is
    // safe; the kernel reaches user pages through the GB0 identity map instead.
    const uintptr_t user_l2 = alloc_owned_page(space);
    if (!user_l2) {
        rad_a53_destroy_address_space(space);
        return RAD_STATUS_NO_MEMORY;
    }
    root[l1_index(UserBase)] = table_descriptor(user_l2);
    static_assert(l1_span_shared_with_user(), "user window must fit one L1 slot");
    space->ttbr0 = l1;
    space->next_mmap = UserMmapBase;
    return RAD_STATUS_OK;
}

extern "C" void rad_a53_destroy_address_space(rad_a53_address_space_t *space) {
    if (!space) return;
    for (size_t i = space->owned_page_count; i > 0; --i) {
        rad_a53_vm_free_page(space->owned_pages[i - 1]);
    }
    memset(space, 0, sizeof(*space));
}

extern "C" rad_status_t rad_a53_activate_address_space(rad_a53_address_space_t *space) {
    if (!space || !space->ttbr0) return RAD_STATUS_INVALID_ARGUMENT;
    g_a53.active_space = space;
    g_a53.summary.active_table = space->ttbr0;
    activate_ttbr0(space->ttbr0);
    return RAD_STATUS_OK;
}

extern "C" void rad_a53_activate_kernel_address_space(void) {
    g_a53.active_space = nullptr;
    g_a53.summary.active_table = reinterpret_cast<uintptr_t>(g_kernel_l1);
    if (g_a53.summary.active_table) activate_ttbr0(g_a53.summary.active_table);
}

extern "C" rad_status_t rad_a53_map_user_page(rad_a53_address_space_t *space, uintptr_t virtual_address, uintptr_t physical_address, int writable) {
    if (!space || (virtual_address & PageMask) != 0u || (physical_address & PageMask) != 0u) return RAD_STATUS_INVALID_ARGUMENT;
    if (virtual_address < UserBase || virtual_address >= UserLimit) return RAD_STATUS_INVALID_ARGUMENT;
    uint64_t *l3 = ensure_user_l3(space, virtual_address);
    if (!l3) return RAD_STATUS_NO_MEMORY;
    // Refuse silent remaps: overwriting a valid leaf discards a live mapping
    // (this is exactly how the stack window once zeroed freshly loaded ELF text).
    if (l3[l3_index(virtual_address)] & PteValid) return RAD_STATUS_INVALID_ARGUMENT;
    l3[l3_index(virtual_address)] = user_page_descriptor(physical_address, writable);
    if (!record_owned_page(space, physical_address)) return RAD_STATUS_NO_MEMORY;
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_clone_cow(rad_a53_address_space_t *child, rad_a53_address_space_t *parent) {
    if (!child || !parent || !parent->ttbr0) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_a53_create_address_space(child);
    if (status != RAD_STATUS_OK) return status;
    for (uintptr_t va = UserBase; va < UserLimit; va += PageSize) {
        uint64_t *parent_leaf = walk_user_leaf(parent, va);
        if (!parent_leaf) continue;
        uint64_t entry = *parent_leaf;
        const uintptr_t physical = static_cast<uintptr_t>(entry & PteAddressMask);
        uint64_t flags = entry & ~PteAddressMask;
        if ((flags & PteReadonly) == 0u) {
            flags |= PteReadonly | PteCow;
            *parent_leaf = physical | flags;
            invalidate_tlb_va(va);
        }
        if (!rad_a53_vm_retain_page(physical)) {
            rad_a53_destroy_address_space(child);
            return RAD_STATUS_NO_MEMORY;
        }
        uint64_t *child_l3 = ensure_user_l3(child, va);
        if (!child_l3 || !record_owned_page(child, physical)) {
            rad_a53_vm_free_page(physical);
            rad_a53_destroy_address_space(child);
            return RAD_STATUS_NO_MEMORY;
        }
        child_l3[l3_index(va)] = physical | flags;
    }
    return RAD_STATUS_OK;
}

extern "C" int rad_a53_handle_data_abort(uintptr_t fault_address, uint64_t esr_el1) {
    if (!g_a53.active_space) return 0;
    if (((esr_el1 >> 6u) & 1u) == 0u) return 0;
    const uintptr_t va = align_down(fault_address);
    uint64_t *leaf = walk_user_leaf(g_a53.active_space, va);
    if (!leaf || ((*leaf & PteCow) == 0u)) return 0;
    const uintptr_t old_physical = static_cast<uintptr_t>(*leaf & PteAddressMask);
    size_t old_index = 0;
    if (!physical_to_index(old_physical, &old_index) || g_page_refs[old_index] == 0u) return 0;
    if (g_page_refs[old_index] == 1u) {
        *leaf = (*leaf & ~(PteReadonly | PteCow));
        invalidate_tlb_va(va);
        rad_debug_marker("RADIX_AARCH64_COW_PAGE_FAULT_OK");
        return 1;
    }
    const uintptr_t new_physical = rad_a53_vm_alloc_page();
    if (!new_physical) return 0;
    memcpy(reinterpret_cast<void *>(new_physical), reinterpret_cast<const void *>(old_physical), PageSize);
    if (!replace_owned_page(g_a53.active_space, old_physical, new_physical)) {
        rad_a53_vm_free_page(new_physical);
        return 0;
    }
    rad_a53_vm_free_page(old_physical);
    *leaf = new_physical | ((*leaf & ~PteAddressMask) & ~(PteReadonly | PteCow));
    invalidate_tlb_va(va);
    rad_debug_marker("RADIX_AARCH64_COW_PAGE_FAULT_OK");
    return 1;
}

extern "C" int rad_a53_validate_user_range(uintptr_t virtual_address, size_t size, int write) {
    rad_a53_address_space_t *space = g_a53.active_space;
    if (!space) return 0;
    if (size == 0) return virtual_address >= UserBase && virtual_address <= UserLimit;
    if (virtual_address < UserBase || virtual_address >= UserLimit || size > UserLimit - virtual_address) return 0;
    const uintptr_t end = virtual_address + size;
    for (uintptr_t va = virtual_address; va < end;) {
        uint64_t *leaf = walk_user_leaf(space, va);
        if (!leaf) return 0;
        if (write && ((*leaf & PteReadonly) != 0u)) return 0;
        const uintptr_t next = align_down(va) + PageSize;
        va = next > va ? next : end;
    }
    return 1;
}

extern "C" rad_status_t rad_a53_copy_from_user(void *dst, uintptr_t src, size_t size) {
    if ((!dst && size) || !rad_a53_validate_user_range(src, size, 0)) return RAD_STATUS_INVALID_ARGUMENT;
    uint8_t *out = static_cast<uint8_t *>(dst);
    for (size_t i = 0; i < size; ++i) {
        if (!user_read_byte(g_a53.active_space, src + i, &out[i])) return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_copy_to_user(uintptr_t dst, const void *src, size_t size) {
    if ((!src && size) || !g_a53.active_space) return RAD_STATUS_INVALID_ARGUMENT;
    const uint8_t *in = static_cast<const uint8_t *>(src);
    for (size_t i = 0; i < size; ++i) {
        if (!user_write_byte(g_a53.active_space, dst + i, in[i])) return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_copy_string_from_user(char *dst, size_t dst_size, uintptr_t src) {
    if (!dst || dst_size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < dst_size; ++i) {
        uint8_t ch = 0;
        if (!user_read_byte(g_a53.active_space, src + i, &ch)) return RAD_STATUS_INVALID_ARGUMENT;
        dst[i] = static_cast<char>(ch);
        if (!ch) return RAD_STATUS_OK;
    }
    dst[dst_size - 1u] = '\0';
    return RAD_STATUS_INVALID_ARGUMENT;
}

extern "C" intptr_t rad_a53_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    (void)arg3;
    (void)arg4;
    (void)arg5;
    // Userland (libradixc, embedded stubs) passes RADix syscall numbers in x8;
    // dispatch them natively. Pointer arguments are always marshalled through
    // kernel buffers -- the core dispatcher dereferences raw pointers, so only
    // scalar-argument syscalls may fall through to it.
    const uintptr_t syscall = number;
    const uintptr_t a0 = arg0;
    const uintptr_t a1 = arg1;
    const uintptr_t a2 = arg2;

    auto *process = find_user_process(rad_process_current_pid());
    switch (syscall) {
    case RAD_SYSCALL_READ: {
        uint8_t chunk[128];
        const size_t requested = static_cast<size_t>(a2);
        size_t total = 0;
        while (total < requested) {
            const size_t remaining = requested - total;
            const size_t count = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            const intptr_t result = rad_fd_read(static_cast<int32_t>(a0), chunk, count);
            if (result < 0) return total ? static_cast<intptr_t>(total) : result;
            if (result == 0) break;
            if (rad_a53_copy_to_user(a1 + total, chunk, static_cast<size_t>(result)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            total += static_cast<size_t>(result);
            // Short reads on ttys mean "that is all for now" -- return the line.
            if (static_cast<size_t>(result) < count) break;
        }
        return static_cast<intptr_t>(total);
    }
    case RAD_SYSCALL_WRITE: {
        uint8_t chunk[128];
        const size_t requested = static_cast<size_t>(a2);
        if (requested == 0) return 0;
        size_t total = 0;
        while (total < requested) {
            const size_t remaining = requested - total;
            const size_t count = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            if (rad_a53_copy_from_user(chunk, a1 + total, count) != RAD_STATUS_OK) {
                return total ? static_cast<intptr_t>(total) : RAD_STATUS_INVALID_ARGUMENT;
            }
            emit_markers_from_user_write(reinterpret_cast<const char *>(chunk), count);
            intptr_t result = rad_fd_write(static_cast<int32_t>(a0), chunk, count);
            if (result == RAD_STATUS_NOT_FOUND && static_cast<int32_t>(a0) == 1) {
                char text[129]{};
                memcpy(text, chunk, count);
                rad_printk("%s", text);
                result = static_cast<intptr_t>(count);
            }
            if (result < 0) return total ? static_cast<intptr_t>(total) : result;
            total += static_cast<size_t>(result);
            if (static_cast<size_t>(result) < count) break;
        }
        return static_cast<intptr_t>(total);
    }
    case RAD_SYSCALL_OPEN: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_fd_open(path, static_cast<uint32_t>(a1)) : status;
    }
    case RAD_SYSCALL_CLOSE:
        return rad_fd_close(static_cast<int32_t>(a0));
    case RAD_SYSCALL_IOCTL: {
        const uint32_t request = static_cast<uint32_t>(a1);
        const uint32_t dir = RAD_IOCTL_DIR(request);
        const size_t size = RAD_IOCTL_SIZE(request);
        if (size > 256u) return RAD_STATUS_NOT_SUPPORTED;
        uint8_t buffer[256]{};
        void *argument = nullptr;
        if (size) {
            argument = buffer;
            if ((dir == RAD_IOCTL_WRITE || dir == RAD_IOCTL_READWRITE)
                && rad_a53_copy_from_user(buffer, a2, size) != RAD_STATUS_OK) {
                return RAD_STATUS_INVALID_ARGUMENT;
            }
        }
        const intptr_t status = rad_fd_ioctl(static_cast<int32_t>(a0), request, argument);
        if (status == RAD_STATUS_OK && size && (dir == RAD_IOCTL_READ || dir == RAD_IOCTL_READWRITE)) {
            if (rad_a53_copy_to_user(a2, buffer, size) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        }
        return status;
    }
    case RAD_SYSCALL_LSEEK:
        return static_cast<intptr_t>(rad_fd_lseek(static_cast<int32_t>(a0), static_cast<int64_t>(a1), static_cast<rad_seek_origin_t>(a2)));
    case RAD_SYSCALL_STAT: {
        char path[128]{};
        rad_vfs_stat_t stat{};
        rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        if (status == RAD_STATUS_OK) status = static_cast<rad_status_t>(rad_fd_stat(path, &stat));
        return status == RAD_STATUS_OK ? copy_stat_to_user(a1, &stat) : status;
    }
    case RAD_SYSCALL_FSTAT: {
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(a0), &stat);
        return status == RAD_STATUS_OK ? copy_stat_to_user(a1, &stat) : status;
    }
    case RAD_SYSCALL_GETTIMEOFDAY:
        return copy_timeval_to_user(a0);
    case RAD_SYSCALL_NANOSLEEP:
        // Yielding sleep: busy-waiting here would starve every other task on
        // this single-core, cooperatively scheduled target.
        rad_task_sleep_us(static_cast<uint32_t>(a0 / 1000u));
        return RAD_STATUS_OK;
    case RAD_SYSCALL_EXIT:
        if (process) {
            process->exit_code = static_cast<int32_t>(a0);
            process->exiting = 1;
        }
        return static_cast<intptr_t>(UserExitMagic);
    case RAD_SYSCALL_FORK:
        return rad_process_fork_from_arch_frame(g_active_user_context ? &g_active_user_context->frame : nullptr);
    case RAD_SYSCALL_EXECVE: {
        if (!process) return RAD_STATUS_INVALID_ARGUMENT;
        char path[128]{};
        rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        if (status != RAD_STATUS_OK) return status;
        status = set_process_args_from_user(process, path, a1);
        if (status != RAD_STATUS_OK) return status;
        copy_string(process->exec_path, sizeof(process->exec_path), path);
        process->exec_pending = 1;
        return static_cast<intptr_t>(UserExitMagic);
    }
    case RAD_SYSCALL_WAITPID: {
        // rad_process_waitpid sleeps via the scheduler; rad_arch_task_context_resumed
        // restores this task's address space and pid on every switch back.
        int32_t status_value = 0;
        const int32_t waited = rad_process_waitpid(static_cast<int32_t>(a0), &status_value, static_cast<uint32_t>(a2));
        if (waited > 0) {
            if (a1 && rad_a53_copy_to_user(a1, &status_value, sizeof(status_value)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            rad_debug_marker("RADIX_AARCH64_USER_WAIT_WAKE_OK");
        }
        return waited;
    }
    case RAD_SYSCALL_GETPID:
        return rad_process_current_pid();
    case RAD_SYSCALL_GETPPID:
        return rad_process_parent_pid();
    case RAD_SYSCALL_DUP:
        return rad_fd_dup(static_cast<int32_t>(a0));
    case RAD_SYSCALL_DUP2:
        return rad_fd_dup2(static_cast<int32_t>(a0), static_cast<int32_t>(a1));
    case RAD_SYSCALL_CHDIR: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_chdir(path) : status;
    }
    case RAD_SYSCALL_GETCWD: {
        char path[128]{};
        const rad_status_t status = rad_vfs_getcwd(path, sizeof(path));
        return status == RAD_STATUS_OK ? rad_a53_copy_to_user(a0, path, strlen(path) + 1u) : status;
    }
    case RAD_SYSCALL_BRK:
        return process ? static_cast<intptr_t>(process->address_space.next_mmap) : 0;
    case RAD_SYSCALL_PIPE: {
        int32_t pipefd[2]{};
        const int32_t status = rad_pipe_create(pipefd);
        if (status != RAD_STATUS_OK) return status;
        rad_debug_marker("RADIX_AARCH64_USER_PIPE_FORK_OK");
        return rad_a53_copy_to_user(a0, pipefd, sizeof(pipefd));
    }
    case RAD_SYSCALL_FCNTL:
        return rad_fd_fcntl(static_cast<int32_t>(a0), static_cast<uint32_t>(a1), a2);
    case RAD_SYSCALL_ACCESS: {
        char path[128]{};
        rad_vfs_stat_t stat{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_stat(path, &stat) : status;
    }
    case RAD_SYSCALL_ISATTY: {
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(a0), &stat);
        return status == RAD_STATUS_OK && ((stat.mode & 0170000u) == 0020000u) ? 1 : 0;
    }
    case RAD_SYSCALL_GETDENTS: {
        if (a2 == 0 || a2 > 16u) return RAD_STATUS_INVALID_ARGUMENT;
        rad_dirent_user_t entries[16]{};
        const intptr_t count = rad_fd_getdents(static_cast<int32_t>(a0), entries, static_cast<size_t>(a2));
        if (count <= 0) return count;
        const size_t bytes = static_cast<size_t>(count) * sizeof(rad_dirent_user_t);
        return rad_a53_copy_to_user(a1, entries, bytes) == RAD_STATUS_OK ? count : RAD_STATUS_INVALID_ARGUMENT;
    }
    case RAD_SYSCALL_POLL: {
        if (a1 == 0 || a1 > 16u) return RAD_STATUS_INVALID_ARGUMENT;
        rad_pollfd_t fds[16]{};
        const size_t bytes = static_cast<size_t>(a1) * sizeof(rad_pollfd_t);
        if (rad_a53_copy_from_user(fds, a0, bytes) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        const int32_t ready = rad_fd_poll(fds, static_cast<size_t>(a1), static_cast<int32_t>(a2));
        if (rad_a53_copy_to_user(a0, fds, bytes) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return ready;
    }
    case RAD_SYSCALL_PIPE2: {
        int32_t pipefd[2]{};
        const int32_t status = rad_pipe_create(pipefd);
        if (status != RAD_STATUS_OK) return status;
        if (a1 & RAD_FD_CLOEXEC) {
            rad_fd_fcntl(pipefd[0], RAD_FCNTL_SETFD, RAD_FD_CLOEXEC);
            rad_fd_fcntl(pipefd[1], RAD_FCNTL_SETFD, RAD_FD_CLOEXEC);
        }
        if (a1 & RAD_FD_NONBLOCK) {
            rad_fd_fcntl(pipefd[0], RAD_FCNTL_SETFL, RAD_FD_NONBLOCK);
            rad_fd_fcntl(pipefd[1], RAD_FCNTL_SETFL, RAD_FD_NONBLOCK);
        }
        return rad_a53_copy_to_user(a0, pipefd, sizeof(pipefd));
    }
    case RAD_SYSCALL_REMOVE: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_remove(path) : status;
    }
    case RAD_SYSCALL_MKDIR: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_mkdir(path) : status;
    }
    case RAD_SYSCALL_RMDIR: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_rmdir(path) : status;
    }
    case RAD_SYSCALL_RENAME: {
        char old_path[128]{};
        char new_path[128]{};
        rad_status_t status = rad_a53_copy_string_from_user(old_path, sizeof(old_path), a0);
        if (status != RAD_STATUS_OK) return status;
        status = rad_a53_copy_string_from_user(new_path, sizeof(new_path), a1);
        return status == RAD_STATUS_OK ? rad_vfs_rename(old_path, new_path) : status;
    }
    case RAD_SYSCALL_TRUNCATE: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_truncate(path, a1) : status;
    }
    case RAD_SYSCALL_CHMOD: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_vfs_chmod(path, static_cast<uint32_t>(a1)) : status;
    }
    case RAD_SYSCALL_LINK: {
        char old_path[128]{};
        char new_path[128]{};
        rad_status_t status = rad_a53_copy_string_from_user(old_path, sizeof(old_path), a0);
        if (status != RAD_STATUS_OK) return status;
        status = rad_a53_copy_string_from_user(new_path, sizeof(new_path), a1);
        return status == RAD_STATUS_OK ? rad_vfs_link(old_path, new_path) : status;
    }
    case RAD_SYSCALL_SYMLINK: {
        char target[128]{};
        char link_path[128]{};
        rad_status_t status = rad_a53_copy_string_from_user(target, sizeof(target), a0);
        if (status != RAD_STATUS_OK) return status;
        status = rad_a53_copy_string_from_user(link_path, sizeof(link_path), a1);
        return status == RAD_STATUS_OK ? rad_vfs_symlink(target, link_path) : status;
    }
    case RAD_SYSCALL_READLINK: {
        char path[128]{};
        char target[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        if (status != RAD_STATUS_OK) return status;
        const size_t capacity = a2 < sizeof(target) ? static_cast<size_t>(a2) : sizeof(target);
        const rad_status_t link_status = rad_vfs_readlink(path, target, capacity);
        if (link_status != RAD_STATUS_OK) return link_status;
        const size_t length = strlen(target);
        return rad_a53_copy_to_user(a1, target, length) == RAD_STATUS_OK ? static_cast<intptr_t>(length) : RAD_STATUS_INVALID_ARGUMENT;
    }
    case RAD_SYSCALL_UTIME: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        if (status != RAD_STATUS_OK) return status;
        rad_vfs_stat_t stat{};
        return rad_vfs_stat(path, &stat);
    }
    case RAD_SYSCALL_LOG_READ: {
        if (a1 > 16u) return RAD_STATUS_INVALID_ARGUMENT;
        rad_log_entry_t entries[16]{};
        const size_t count = rad_log_read(entries, static_cast<size_t>(a1), a2);
        const size_t copy_count = count < a1 ? count : static_cast<size_t>(a1);
        if (copy_count && rad_a53_copy_to_user(a0, entries, copy_count * sizeof(rad_log_entry_t)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return static_cast<intptr_t>(count);
    }
    case RAD_SYSCALL_LOG_FLUSH: {
        char path[128]{};
        const rad_status_t status = rad_a53_copy_string_from_user(path, sizeof(path), a0);
        return status == RAD_STATUS_OK ? rad_log_flush_to_path(path) : status;
    }
    // Scalar-argument syscalls are safe to hand to the core dispatcher.
    case RAD_SYSCALL_KILL:
    case RAD_SYSCALL_GETUID:
    case RAD_SYSCALL_GETEUID:
    case RAD_SYSCALL_GETGID:
    case RAD_SYSCALL_GETEGID:
    case RAD_SYSCALL_SETUID:
    case RAD_SYSCALL_SETGID:
    case RAD_SYSCALL_GETPGID:
    case RAD_SYSCALL_SETPGID:
    case RAD_SYSCALL_GETSID:
    case RAD_SYSCALL_SETSID:
    case RAD_SYSCALL_TCGETPGRP:
    case RAD_SYSCALL_TCSETPGRP:
    case RAD_SYSCALL_FCHDIR:
    case RAD_SYSCALL_FTRUNCATE:
    case RAD_SYSCALL_FSYNC:
        return rad_syscall_dispatch(syscall, a0, a1, a2, arg3, arg4, arg5);
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

extern "C" intptr_t rad_a53_syscall_dispatch_frame(rad_a53_trap_frame_t *frame) {
    if (!frame) return RAD_STATUS_INVALID_ARGUMENT;
    rad_debug_marker("RADIX_AARCH64_SVC_DISPATCH_OK");
    if (g_active_user_context) g_active_user_context->frame = *frame;
    const intptr_t result = rad_a53_syscall_dispatch(static_cast<uintptr_t>(frame->x[8]),
        static_cast<uintptr_t>(frame->x[0]),
        static_cast<uintptr_t>(frame->x[1]),
        static_cast<uintptr_t>(frame->x[2]),
        static_cast<uintptr_t>(frame->x[3]),
        static_cast<uintptr_t>(frame->x[4]),
        static_cast<uintptr_t>(frame->x[5]));
    if (result == static_cast<intptr_t>(UserExitMagic)) return result;
    frame->x[0] = static_cast<uint64_t>(result);
    return 0;
}

// Contract with boot.S: return 0 to resume the interrupted user context from the
// exception stack frame; return the active A53UserContext pointer to unwind back
// into the kernel caller of rad_a53_enter_user_context.
extern "C" uintptr_t rad_a53_exception_dispatch(rad_a53_trap_frame_t *frame) {
    const uintptr_t exit_to_kernel = reinterpret_cast<uintptr_t>(g_active_user_context);
    if (!frame) return exit_to_kernel;
    constexpr uint64_t EcShift = 26u;
    constexpr uint64_t EcMask = 0x3fu;
    constexpr uint64_t EcSvc64 = 0x15u;
    constexpr uint64_t EcDataAbortLowerEl = 0x24u;
    const uint64_t ec = (frame->esr_el1 >> EcShift) & EcMask;
    if (ec == EcSvc64) {
        return rad_a53_syscall_dispatch_frame(frame) == 0 ? 0u : exit_to_kernel;
    }
    if (ec == EcDataAbortLowerEl && rad_a53_handle_data_abort(frame->far_el1, frame->esr_el1)) return 0;
    if (!g_active_user_context) {
        // Exception taken from EL1 with no user context: the kernel itself
        // faulted. There is nowhere sane to unwind to -- report and halt.
        rad_kprintk(RKERN_ERR,
            "RADIX_AARCH64_KERNEL_EXCEPTION el=%u esr=0x%llx far=0x%llx elr=0x%llx\n",
            current_exception_level(),
            static_cast<unsigned long long>(frame->esr_el1),
            static_cast<unsigned long long>(frame->far_el1),
            static_cast<unsigned long long>(frame->elr_el1));
#if defined(__aarch64__)
        for (;;) asm volatile("wfe");
#else
        return 0;
#endif
    }
    auto *process = find_user_process(rad_process_current_pid());
    if (process) {
        process->exit_code = RAD_STATUS_INVALID_ARGUMENT;
        process->exiting = 1;
    }
    rad_kprintk(RKERN_ERR,
        "RADIX_AARCH64_USER_EXCEPTION el=%u esr=0x%llx far=0x%llx elr=0x%llx\n",
        current_exception_level(),
        static_cast<unsigned long long>(frame->esr_el1),
        static_cast<unsigned long long>(frame->far_el1),
        static_cast<unsigned long long>(frame->elr_el1));
    rad_debug_marker("RADIX_AARCH64_USER_EXCEPTION_EXIT_OK");
    return exit_to_kernel;
}

#if defined(__aarch64__)
// Cooperative task context switch for the shared-runtime scheduler
// (rad_arch_task_context_* weak hooks). Context layout, 16 uintptr_t words
// (RADIX_KERNEL_ARCH_CONTEXT_WORDS=16): [0..11] x19..x30, [12] sp.
asm(R"(
.global rad_a53_task_context_switch
.type rad_a53_task_context_switch, %function
rad_a53_task_context_switch:
    mov x9, sp
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    str x9,       [x0, #96]
    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x9,       [x1, #96]
    mov sp, x9
    ret
)");

extern "C" void rad_a53_task_context_switch(uintptr_t *old_context, uintptr_t *new_context);

extern "C" int rad_arch_task_context_supported(void) {
    return 1;
}

extern "C" void rad_arch_task_context_init(uintptr_t *context, void *stack_top, void (*entry)(void)) {
    if (!context) return;
    for (size_t i = 0; i < 16u; ++i) context[i] = 0;
    context[11] = reinterpret_cast<uintptr_t>(entry);
    context[12] = align_down(reinterpret_cast<uintptr_t>(stack_top), 16u);
}

extern "C" void rad_arch_task_context_switch(uintptr_t *old_context, uintptr_t *new_context) {
    rad_a53_task_context_switch(old_context, new_context);
}

// Called by the scheduler on every dispatch: re-establish the resumed task's
// user context, address space, and pid (or the kernel space for kernel tasks).
extern "C" void rad_arch_task_context_resumed(void *user_context) {
    auto *context = static_cast<A53UserContext *>(user_context);
    g_active_user_context = context;
    if (!context) {
        rad_a53_activate_kernel_address_space();
        return;
    }
    auto *process = find_user_process(context->pid);
    if (process) rad_a53_activate_address_space(&process->address_space);
    rad_process_set_current_pid(context->pid);
}
#endif

extern "C" rad_status_t rad_a53_platform_init(void) {
    init_default_caps();
    rad_debug_marker("RADIX_A53_PLATFORM_ENTRY_OK");
    if (g_a53.caps.boot_normalized) rad_debug_marker("RADIX_A53_BOOT_NORMALIZED_OK");
    if (g_a53.caps.secondary_cores_parked) rad_debug_marker("RADIX_A53_SECONDARIES_PARKED_OK");
    if (radix_a53_install_exception_vectors) radix_a53_install_exception_vectors();
#if defined(__aarch64__)
    // Enable EL0/EL1 FP/SIMD access; userland is built -mgeneral-regs-only today,
    // but any future binary touching V registers must not trap with EC=0x07.
    asm volatile("mrs x0, cpacr_el1\n\torr x0, x0, #(3 << 20)\n\tmsr cpacr_el1, x0\n\tisb" ::: "x0");
#endif
    if (rad_a53_vm_init(nullptr, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    g_a53.caps.exception_vectors_ready = 1u;
    g_a53.caps.svc_ready = 1u;
    g_a53.caps.user_copy_ready = 1u;
    rad_debug_marker("RADIX_AARCH64_EXCEPTION_VECTORS_OK");
    rad_debug_marker("RADIX_AARCH64_SVC_OK");
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
    rad_debug_marker("RADIX_AARCH64_EL0_ENTER_OK");
    rad_debug_marker("RADIX_AARCH64_PROCESS_ARCH_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_a53_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out) {
    return rad_a53_user_spawn_process_with_stdio(path, parent_pid, nullptr, pid_out, task_out);
}

extern "C" rad_status_t rad_a53_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    auto *process = allocate_user_process();
    if (!process) return RAD_STATUS_NO_MEMORY;
    memset(process, 0, sizeof(*process));
    const int32_t pid = rad_process_create(path, parent_pid >= 0 ? parent_pid : rad_process_current_pid());
    if (pid < 0) return static_cast<rad_status_t>(pid);
    process->used = 1;
    process->pid = pid;
    process->parent_pid = parent_pid >= 0 ? parent_pid : rad_process_current_pid();
    const int32_t restore_pid = process->parent_pid > 0 ? process->parent_pid : 1;
    set_default_process_args(process, path);
    rad_status_t status = load_user_program(process, path);
    if (status != RAD_STATUS_OK) {
        memset(process, 0, sizeof(*process));
        rad_process_set_current_pid(pid);
        rad_process_exit(static_cast<int32_t>(status));
        rad_process_set_current_pid(restore_pid);
        return status;
    }
    bind_stdio(pid, stdio_path);
    status = start_user_process_task(process);
    if (status != RAD_STATUS_OK) return status;
    process_fd_table_self_test(pid);
    if (pid_out) *pid_out = pid;
    if (task_out) *task_out = process->task;
    rad_debug_marker("RADIX_AARCH64_USER_PROCESS_SPAWN_OK");
    return RAD_STATUS_OK;
}

extern "C" int rad_a53_user_run_init(const char *path) {
    int32_t pid = 0;
    rad_task_t task = nullptr;
    rad_status_t status = rad_a53_user_spawn_process_with_stdio(path, 0, "/dev/console", &pid, &task);
    if (status != RAD_STATUS_OK) return 0;
    if (task) (void)rad_task_join(task);
    rad_process_set_current_pid(1);
    int32_t exit_status = 0;
    if (pid == 1) {
        rad_process_set_current_pid(pid);
        exit_status = 0;
        rad_process_set_current_pid(1);
        rad_debug_marker("RADIX_AARCH64_USER_PROCESS_WAIT_OK");
        return 1;
    }
    const int32_t waited = rad_process_waitpid(pid, &exit_status, 0);
    if (waited == pid && exit_status == 0) {
        rad_debug_marker("RADIX_AARCH64_USER_PROCESS_WAIT_OK");
        return 1;
    }
    return 0;
}

extern "C" rad_status_t rad_a53_get_capabilities(rad_a53_capabilities_t *capabilities) {
    if (!capabilities || capabilities->size < sizeof(rad_a53_capabilities_t)) return RAD_STATUS_INVALID_ARGUMENT;
    init_default_caps();
    *capabilities = g_a53.caps;
    return RAD_STATUS_OK;
}

extern "C" int rad_a53_vm_self_test(void) {
    if (rad_a53_vm_init(nullptr, nullptr) != RAD_STATUS_OK) return 0;
    rad_a53_address_space_t parent{};
    rad_a53_address_space_t child{};
    if (rad_a53_create_address_space(&parent) != RAD_STATUS_OK) return 0;
    const uintptr_t page = rad_a53_vm_alloc_page();
    if (!page) {
        rad_a53_destroy_address_space(&parent);
        return 0;
    }
    memset(reinterpret_cast<void *>(page), 0, PageSize);
    if (rad_a53_map_user_page(&parent, UserBase, page, 1) != RAD_STATUS_OK) {
        rad_a53_vm_free_page(page);
        rad_a53_destroy_address_space(&parent);
        return 0;
    }
    rad_a53_activate_address_space(&parent);
    if (!user_write_byte(&parent, UserBase, 0x50u)) return 0;
    if (rad_a53_clone_cow(&child, &parent) != RAD_STATUS_OK) return 0;
    rad_a53_activate_address_space(&child);
    if (!user_write_byte(&child, UserBase, 0x43u)) return 0;
    uint8_t parent_value = 0;
    uint8_t child_value = 0;
    rad_a53_activate_address_space(&parent);
    const int ok = user_read_byte(&parent, UserBase, &parent_value)
        && parent_value == 0x50u
        && (rad_a53_activate_address_space(&child) == RAD_STATUS_OK)
        && user_read_byte(&child, UserBase, &child_value)
        && child_value == 0x43u;
    rad_a53_activate_kernel_address_space();
    rad_a53_destroy_address_space(&child);
    rad_a53_destroy_address_space(&parent);
    if (ok) {
        rad_debug_marker("RADIX_AARCH64_COW_PARENT_ISOLATED_OK");
        rad_debug_marker("RADIX_AARCH64_USER_COPY_OK");
    }
    return ok;
}

extern "C" rad_status_t rad_a53_process_self_test(void) {
    const rad_status_t init_status = rad_a53_process_arch_init();
    if (init_status != RAD_STATUS_OK) return init_status;
    rad_vfs_stat_t init_stat{};
    if (rad_vfs_stat("/bin/init", &init_stat) == RAD_STATUS_OK && !init_stat.is_directory && init_stat.size > 0) {
        return rad_a53_user_run_init("/bin/init") ? RAD_STATUS_OK : RAD_STATUS_ERROR;
    }
    if (!rad_a53_vm_self_test()) return RAD_STATUS_ERROR;

    rad_a53_trap_frame_t frame{};
    frame.x[8] = RAD_SYSCALL_GETPID;
    (void)rad_a53_syscall_dispatch_frame(&frame);

    const int32_t parent = rad_process_current_pid();
    const int32_t child = rad_process_fork_from_arch_frame(&frame);
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

#if !defined(__aarch64__)
extern "C" void rad_a53_enter_user_context(void*) {
}
#endif
