#include "x86_cpu.h"

#include <radkernel/radkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" void x86_load_gdt(void *descriptor);
extern "C" void x86_load_idt(void *descriptor);
extern "C" void x86_cpu_init_ap(uint32_t core);
extern "C" void x86_int80_entry(void);
extern "C" void x86_irq_32_entry(void);
extern "C" void x86_irq_33_entry(void);
extern "C" void x86_irq_34_entry(void);
extern "C" void x86_irq_35_entry(void);
extern "C" void x86_irq_36_entry(void);
extern "C" void x86_irq_37_entry(void);
extern "C" void x86_irq_38_entry(void);
extern "C" void x86_irq_39_entry(void);
extern "C" void x86_irq_40_entry(void);
extern "C" void x86_irq_41_entry(void);
extern "C" void x86_irq_42_entry(void);
extern "C" void x86_irq_43_entry(void);
extern "C" void x86_irq_44_entry(void);
extern "C" void x86_irq_45_entry(void);
extern "C" void x86_irq_46_entry(void);
extern "C" void x86_irq_47_entry(void);
extern "C" void x86_lapic_timer_entry(void);
extern "C" void x86_exception_no_error_entry(void);
extern "C" void x86_exception_error_entry(void);
extern "C" void x86_invalid_opcode_entry(void);
extern "C" void x86_device_not_available_entry(void);
extern "C" void x86_double_fault_entry(void);
extern "C" void x86_invalid_tss_entry(void);
extern "C" void x86_floating_point_entry(void);
extern "C" void x86_simd_floating_point_entry(void);
extern "C" void x86_alignment_check_entry(void);
extern "C" void x86_control_protection_entry(void);
extern "C" void x86_segment_not_present_entry(void);
extern "C" void x86_stack_segment_entry(void);
extern "C" void x86_general_protection_entry(void);
extern "C" void x86_page_fault_entry(void);
extern "C" char x86_boot_stack_guard[]; // guard region below the boot stack, made non-present
// Size of the boot-stack guard region. Must match the `.skip` after x86_boot_stack_guard in
// boot.S. Multiple pages so a single large stack frame cannot step over the guard.
static constexpr uint64_t kBootStackGuardBytes = 32768u; // 8 pages
extern "C" void x86_syscall_entry(void);
extern "C" intptr_t x86_test_int80_getpid(void);
extern "C" void x86_context_switch(void *old_context, const void *new_context);
extern "C" uint8_t x86_ap_trampoline_start[];
extern "C" uint8_t x86_ap_trampoline_end[];
extern "C" uint8_t x86_boot_pml4[];
extern "C" uint32_t x86_ap_trampoline_pml4_offset;
extern "C" uint32_t x86_ap_trampoline_core_offset;
extern "C" uint32_t x86_ap_trampoline_stack_offset;
extern "C" uint32_t x86_ap_trampoline_entry_offset;
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);
extern "C" void rad_arch_scheduler_tick(uint32_t core);

namespace {
constexpr uint16_t KernelCode = 0x08;
constexpr uint16_t KernelData = 0x10;
constexpr uint16_t UserData = 0x1b;
constexpr uint16_t UserCode = 0x23;
constexpr uint16_t TssSelector = 0x28;
constexpr uint8_t PicMasterCommand = 0x20;
constexpr uint8_t PicMasterData = 0x21;
constexpr uint8_t PicSlaveCommand = 0xa0;
constexpr uint8_t PicSlaveData = 0xa1;
constexpr uint8_t PicEoi = 0x20;
constexpr uint8_t PicMasterVectorBase = 0x20;
constexpr uint8_t PicSlaveVectorBase = 0x28;

constexpr uint32_t MsrEfer = 0xc0000080u;
constexpr uint32_t MsrStar = 0xc0000081u;
constexpr uint32_t MsrLstar = 0xc0000082u;
constexpr uint32_t MsrSfmask = 0xc0000084u;
constexpr uintptr_t ApTrampolineAddress = 0x7000u;
constexpr uint8_t ApTrampolineVector = static_cast<uint8_t>(ApTrampolineAddress >> 12);
constexpr uint32_t MaxX86Cores = 4u;
constexpr size_t ApStackSize = 16384u;
constexpr uint32_t LapicId = 0x20u;
constexpr uint32_t LapicEoi = 0xb0u;
constexpr uint32_t LapicSvr = 0xf0u;
constexpr uint32_t LapicLvtTimer = 0x320u;
constexpr uint32_t LapicIcrLow = 0x300u;
constexpr uint32_t LapicIcrHigh = 0x310u;
constexpr uint32_t LapicTimerInitialCount = 0x380u;
constexpr uint32_t LapicTimerDivide = 0x3e0u;
constexpr uint8_t LapicTimerVector = 0xefu;
constexpr uint32_t LapicTimerPeriodic = 1u << 17;
constexpr uint32_t LapicTimerDivideBy16 = 0x3u;
constexpr uint32_t LapicTimerInitialCountValue = 1000000u;

struct [[gnu::packed]] DescriptorPtr {
    uint16_t limit;
    uint64_t base;
};

struct [[gnu::packed]] Tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

struct [[gnu::packed]] IdtGate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct X86Context {
    uint64_t rsp;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
};

alignas(16) uint64_t g_gdt[MaxX86Cores][7];
Tss g_tss[MaxX86Cores]{};
IdtGate g_idt[256]{};
uint16_t g_pic_mask = 0xffffu;
uint32_t g_detected_cores = 1;
uint32_t g_present_core_mask = 1;
uint64_t g_lapic_address = 0xfee00000ull;
X86Context g_context_probe_main{};
X86Context g_context_probe_task{};
alignas(16) uint8_t g_context_probe_stack[4096];
volatile uint32_t g_context_probe_stage = 0;
alignas(16) uint8_t g_ap_stacks[MaxX86Cores][ApStackSize];
// Per-core stacks for the fatal-exception vectors (#DF/#GP), reached via the TSS
// IST so they run even when the main kernel stack is exhausted -- e.g. when a
// stack overflow hits the boot-stack guard page.
alignas(16) uint8_t g_ist_stacks[MaxX86Cores][ApStackSize];
// A 4 KiB page table that splits the 2 MiB identity page covering the boot-stack
// guard, so the single guard page can be made non-present (x86_install_stack_guard).
alignas(4096) uint64_t g_stack_guard_pt[512];
void (*g_ap_worker_entries[MaxX86Cores])(uint32_t core){};
volatile uint32_t g_ap_started_mask = 1u;
volatile uint32_t g_ap_starting_core = 0;
volatile uint64_t g_lapic_timer_irqs[MaxX86Cores]{};
volatile uint32_t g_lapic_timer_started_mask = 0;
volatile uint32_t g_lapic_timer_marker_seen = 0;
volatile uint32_t g_ap_lapic_timer_marker_seen = 0;

void (*const g_irq_entries[16])() = {
    x86_irq_32_entry,
    x86_irq_33_entry,
    x86_irq_34_entry,
    x86_irq_35_entry,
    x86_irq_36_entry,
    x86_irq_37_entry,
    x86_irq_38_entry,
    x86_irq_39_entry,
    x86_irq_40_entry,
    x86_irq_41_entry,
    x86_irq_42_entry,
    x86_irq_43_entry,
    x86_irq_44_entry,
    x86_irq_45_entry,
    x86_irq_46_entry,
    x86_irq_47_entry,
};

uint64_t make_code_data_descriptor(uint32_t access, uint32_t flags) {
    return 0x000000000000ffffull
        | (static_cast<uint64_t>(access) << 40)
        | (static_cast<uint64_t>(flags) << 52);
}

void set_tss_descriptor(uint32_t core, uint16_t selector, uintptr_t base, uint32_t limit) {
    if (core >= MaxX86Cores) core = 0;
    const size_t index = selector >> 3;
    uint64_t low = (limit & 0xffffu)
        | ((base & 0xffffffull) << 16)
        | (0x89ull << 40)
        | (((limit >> 16) & 0x0full) << 48)
        | (((base >> 24) & 0xffull) << 56);
    uint64_t high = base >> 32;
    g_gdt[core][index] = low;
    g_gdt[core][index + 1] = high;
}

void set_idt_gate(uint8_t vector, void (*handler)(), uint8_t attributes) {
    const uintptr_t offset = reinterpret_cast<uintptr_t>(handler);
    IdtGate& gate = g_idt[vector];
    gate.offset_low = static_cast<uint16_t>(offset & 0xffffu);
    gate.selector = KernelCode;
    gate.ist = 0;
    gate.attributes = attributes;
    gate.offset_mid = static_cast<uint16_t>((offset >> 16) & 0xffffu);
    gate.offset_high = static_cast<uint32_t>((offset >> 32) & 0xffffffffu);
    gate.zero = 0;
}

void write_msr(uint32_t msr, uint64_t value) {
    const uint32_t lo = static_cast<uint32_t>(value);
    const uint32_t hi = static_cast<uint32_t>(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

uint64_t read_msr(uint32_t msr) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

uint64_t read_cr2(void) {
    uint64_t value = 0;
    asm volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

uint64_t read_cr3(void) {
    uint64_t value = 0;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

void write_cr3(uint64_t value) {
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

volatile uint32_t *lapic_base(void) {
    return reinterpret_cast<volatile uint32_t *>(static_cast<uintptr_t>(g_lapic_address));
}

uint32_t lapic_read(uint32_t reg) {
    return lapic_base()[reg / 4u];
}

void lapic_write(uint32_t reg, uint32_t value) {
    lapic_base()[reg / 4u] = value;
    (void)lapic_read(LapicId);
}

void lapic_eoi(void) {
    if (g_lapic_address) lapic_write(LapicEoi, 0);
}

void lapic_wait_icr(void) {
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if ((lapic_read(LapicIcrLow) & (1u << 12)) == 0) return;
        asm volatile("pause");
    }
}

void lapic_send_ipi(uint8_t apic_id, uint32_t command) {
    lapic_wait_icr();
    lapic_write(LapicIcrHigh, static_cast<uint32_t>(apic_id) << 24);
    lapic_write(LapicIcrLow, command);
    lapic_wait_icr();
}

void delay_pause(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) asm volatile("pause");
}

void io_wait(void) {
    outb(0x80, 0);
}

void pic_write_masks(void) {
    outb(PicMasterData, static_cast<uint8_t>(g_pic_mask & 0xffu));
    outb(PicSlaveData, static_cast<uint8_t>((g_pic_mask >> 8) & 0xffu));
}

void pic_remap_and_mask(void) {
    asm volatile("cli");
    g_pic_mask = 0xffffu;

    outb(PicMasterCommand, 0x11);
    io_wait();
    outb(PicSlaveCommand, 0x11);
    io_wait();
    outb(PicMasterData, PicMasterVectorBase);
    io_wait();
    outb(PicSlaveData, PicSlaveVectorBase);
    io_wait();
    outb(PicMasterData, 0x04);
    io_wait();
    outb(PicSlaveData, 0x02);
    io_wait();
    outb(PicMasterData, 0x01);
    io_wait();
    outb(PicSlaveData, 0x01);
    io_wait();
    pic_write_masks();
}

void load_task_register(void) {
    asm volatile("ltr %0" : : "r"(TssSelector));
}

void prepare_gdt_tss_for_core(uint32_t core, uint64_t kernel_stack_top) {
    if (core >= MaxX86Cores) core = 0;
    memset(g_gdt[core], 0, sizeof(g_gdt[core]));
    memset(&g_tss[core], 0, sizeof(g_tss[core]));
    g_gdt[core][1] = make_code_data_descriptor(0x9au, 0x0au);
    g_gdt[core][2] = make_code_data_descriptor(0x92u, 0x0au);
    g_gdt[core][3] = make_code_data_descriptor(0xf2u, 0x0au);
    g_gdt[core][4] = make_code_data_descriptor(0xfau, 0x0au);
    g_tss[core].rsp0 = kernel_stack_top;
    // IST slot 1 (TSS.ist[0]) is a dedicated stack for the fatal traps wired to it
    // in the IDT, so they can be delivered and reported even after a stack overflow.
    g_tss[core].ist[0] = reinterpret_cast<uint64_t>(&g_ist_stacks[core][ApStackSize]);
    g_tss[core].iomap_base = sizeof(Tss);
    set_tss_descriptor(core, TssSelector, reinterpret_cast<uintptr_t>(&g_tss[core]), sizeof(Tss) - 1u);
}

void load_gdt_tss_for_core(uint32_t core) {
    if (core >= MaxX86Cores) core = 0;
    DescriptorPtr gdt_ptr{static_cast<uint16_t>(sizeof(g_gdt[core]) - 1u), reinterpret_cast<uint64_t>(g_gdt[core])};
    x86_load_gdt(&gdt_ptr);
    load_task_register();
}

void load_shared_idt(void) {
    DescriptorPtr idt_ptr{static_cast<uint16_t>(sizeof(g_idt) - 1u), reinterpret_cast<uint64_t>(g_idt)};
    x86_load_idt(&idt_ptr);
}

void configure_syscall_msrs(void) {
    const uint64_t efer = read_msr(MsrEfer) | 1u;
    write_msr(MsrEfer, efer);
    const uint64_t star = (static_cast<uint64_t>(0x13u) << 48) | (static_cast<uint64_t>(KernelCode) << 32);
    write_msr(MsrStar, star);
    write_msr(MsrLstar, reinterpret_cast<uintptr_t>(x86_syscall_entry));
    write_msr(MsrSfmask, 0x200u);
}

void enable_sse(void) {
    uint64_t cr0 = 0;
    uint64_t cr4 = 0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ull << 2); // EM: allow FPU/SSE instructions.
    cr0 |= (1ull << 1);  // MP: monitor coprocessor for wait/fwait.
    asm volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ull << 9) | (1ull << 10); // OSFXSR | OSXMMEXCPT.

    // Enable XSAVE + AVX when the CPU supports them. Rust's libm (FMA fma_with_fma path)
    // and portable_atomic (128-bit atomic load/store via VEX vmovdqa) runtime-dispatch to
    // VEX/AVX-encoded instructions whenever CPUID reports AVX. VEX instructions #UD unless
    // the OS sets CR4.OSXSAVE and XCR0[2:1]=11b (SSE|AVX) -- so on real AVX hardware the WM
    // would fault (e.g. #UD in atomic_load) without this. Only XMM (VEX.128) is generated,
    // so the fxsave/fxrstor in the interrupt stubs still preserves the full live state.
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1u), "c"(0u));
    const bool have_xsave = (ecx & (1u << 26)) != 0u;
    const bool have_avx   = (ecx & (1u << 28)) != 0u;
    if (have_xsave) cr4 |= (1ull << 18); // OSXSAVE
    asm volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
    if (have_xsave) {
        uint32_t xcr0_lo = 0x1u | 0x2u;      // x87 | SSE
        if (have_avx) xcr0_lo |= 0x4u;       // AVX (YMM) -- required to legalise VEX encoding
        asm volatile("xsetbv" : : "a"(xcr0_lo), "d"(0u), "c"(0u));
    }
    asm volatile("fninit");
}

uintptr_t align_down_uintptr(uintptr_t value, uintptr_t alignment) {
    return value & ~(alignment - 1u);
}

uint32_t apic_id_for_core(uint32_t core) {
    return core;
}
}

extern "C" void x86_context_probe_entry(void) {
    g_context_probe_stage = 1;
    x86_context_switch(&g_context_probe_task, &g_context_probe_main);
    g_context_probe_stage = 2;
    x86_context_switch(&g_context_probe_task, &g_context_probe_main);
    for (;;) {
        rad_cpu_idle();
    }
}

extern "C" void x86_exception_report(uint64_t vector, uint64_t error, uint64_t rip, uint64_t cs) {
    x86_serial_write("x86 exception vector=");
    char buffer[160];
    snprintf(buffer, sizeof(buffer), "%llu error=0x%llx rip=0x%llx cs=0x%llx mode=%s",
        static_cast<unsigned long long>(vector),
        static_cast<unsigned long long>(error),
        static_cast<unsigned long long>(rip),
        static_cast<unsigned long long>(cs),
        (cs & 3u) == 3u ? "user" : "kernel");
    x86_serial_write(buffer);
    if (vector == 14) {
        uint64_t pf_rsp = 0;
        asm volatile("mov %%rsp, %0" : "=r"(pf_rsp));
        const uint32_t pf_core = x86_cpu_current_core();
        const uint64_t pf_rsp0 = pf_core < MaxX86Cores ? g_tss[pf_core].rsp0 : 0u;
        snprintf(buffer, sizeof(buffer), " cr2=0x%llx present=%u write=%u user=%u rsp=0x%llx tss_rsp0=0x%llx core=%u\n",
            static_cast<unsigned long long>(read_cr2()),
            static_cast<unsigned>((error >> 0) & 1u),
            static_cast<unsigned>((error >> 1) & 1u),
            static_cast<unsigned>((error >> 2) & 1u),
            static_cast<unsigned long long>(pf_rsp),
            static_cast<unsigned long long>(pf_rsp0),
            static_cast<unsigned>(pf_core));
        x86_serial_write(buffer);
    } else {
        // Diagnostic for non-page-fault traps (e.g. #GP): report the current
        // kernel stack pointer and the TSS RSP0 the CPU would load on a
        // ring3->ring0 interrupt, so a fault deep in an IRQ handler chain can be
        // classified as stack overflow (rsp far below rsp0) vs. stack collision.
        uint64_t cur_rsp = 0;
        asm volatile("mov %%rsp, %0" : "=r"(cur_rsp));
        const uint32_t core = x86_cpu_current_core();
        const uint64_t rsp0 = core < MaxX86Cores ? g_tss[core].rsp0 : 0u;
        snprintf(buffer, sizeof(buffer), " rsp=0x%llx tss_rsp0=0x%llx core=%u\n",
            static_cast<unsigned long long>(cur_rsp),
            static_cast<unsigned long long>(rsp0),
            static_cast<unsigned>(core));
        x86_serial_write(buffer);
    }
    // A #PF or #DF whose faulting address lands in the boot-stack guard page is a
    // kernel stack overflow -- name it explicitly rather than leaving a bare trap.
    if (vector == 8u || vector == 14u) {
        const uint64_t cr2 = read_cr2();
        const uint64_t guard = reinterpret_cast<uint64_t>(x86_boot_stack_guard);
        if (cr2 >= guard && cr2 < guard + kBootStackGuardBytes) {
            x86_serial_write("RAD_X86_STACK_GUARD_OVERFLOW kernel boot stack overflowed into the guard page\n");
            rad_debug_marker("RAD_X86_STACK_GUARD_OVERFLOW");
        }
    }
    rad_cpu_halt_forever();
}

extern "C" void x86_irq_dispatch(uint64_t vector) {
    if (vector < PicMasterVectorBase || vector >= PicMasterVectorBase + 16u) return;
    const uint32_t irq = static_cast<uint32_t>(vector - PicMasterVectorBase);
    const rad_status_t status = rad_irq_dispatch(irq);
    if (status != RAD_STATUS_OK && irq < 16u) {
        g_pic_mask |= static_cast<uint16_t>(1u << irq);
        pic_write_masks();
    }
    if (irq >= 8u) outb(PicSlaveCommand, PicEoi);
    outb(PicMasterCommand, PicEoi);
}

extern "C" rad_status_t rad_hal_irq_enable(uint32_t irq) {
    if (irq >= 16u) return RAD_STATUS_NOT_SUPPORTED;
    if (irq >= 8u) g_pic_mask &= static_cast<uint16_t>(~(1u << 2));
    g_pic_mask &= static_cast<uint16_t>(~(1u << irq));
    pic_write_masks();
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_irq_disable(uint32_t irq) {
    if (irq >= 16u) return RAD_STATUS_NOT_SUPPORTED;
    g_pic_mask |= static_cast<uint16_t>(1u << irq);
    pic_write_masks();
    return RAD_STATUS_OK;
}

extern "C" void x86_irq_enable_interrupts(void) {
    rad_cpu_interrupts_enable();
}

extern "C" void x86_cpu_set_topology(uint32_t detected_cores, uint32_t present_mask, uint64_t lapic_address) {
    if (detected_cores == 0) detected_cores = 1;
    g_detected_cores = detected_cores;
    g_present_core_mask = present_mask ? present_mask : 1u;
    if (lapic_address) g_lapic_address = lapic_address;
}

extern "C" uint32_t x86_cpu_detected_core_count(void) {
    return g_detected_cores ? g_detected_cores : 1u;
}

extern "C" uint32_t x86_cpu_present_core_mask(void) {
    return g_present_core_mask ? g_present_core_mask : 1u;
}

extern "C" uint64_t x86_cpu_lapic_address(void) {
    return g_lapic_address;
}

extern "C" uint32_t x86_cpu_current_core(void) {
    if (__atomic_load_n(&g_ap_started_mask, __ATOMIC_ACQUIRE) == 0) return 0;
    if (!g_lapic_address) return 0;
    const uint32_t apic_id = (lapic_read(LapicId) >> 24) & 0xffu;
    for (uint32_t core = 0; core < MaxX86Cores; ++core) {
        if (apic_id_for_core(core) == apic_id) return core;
    }
    return 0;
}

extern "C" uint32_t x86_cpu_started_core_mask(void) {
    return __atomic_load_n(&g_ap_started_mask, __ATOMIC_ACQUIRE);
}

extern "C" void x86_lapic_timer_start_current_core(void) {
    if (!g_lapic_address) return;
    const uint32_t core = x86_cpu_current_core();
    if (core >= MaxX86Cores) return;
    lapic_write(LapicTimerDivide, LapicTimerDivideBy16);
    lapic_write(LapicLvtTimer, LapicTimerVector | LapicTimerPeriodic);
    lapic_write(LapicTimerInitialCount, LapicTimerInitialCountValue);
    __atomic_fetch_or(&g_lapic_timer_started_mask, 1u << core, __ATOMIC_RELEASE);
    if (!__atomic_exchange_n(&g_lapic_timer_marker_seen, 1u, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RAD_LAPIC_TIMER_OK");
    }
}

extern "C" void x86_lapic_timer_dispatch(uint64_t interrupted_cs) {
    const uint32_t core = x86_cpu_current_core();
    if (core < MaxX86Cores) {
        __atomic_fetch_add(&g_lapic_timer_irqs[core], 1u, __ATOMIC_RELAXED);
        if (core > 0 && !__atomic_exchange_n(&g_ap_lapic_timer_marker_seen, 1u, __ATOMIC_ACQ_REL)) {
            rad_debug_marker("RAD_AP_TIMER_IRQ_OK");
        }
        lapic_eoi();
        if ((interrupted_cs & 3u) == 3u) rad_arch_scheduler_tick(core);
        return;
    }
    lapic_eoi();
}

extern "C" void x86_ap_entry(uint32_t core) {
    if (core >= MaxX86Cores) rad_cpu_halt_forever();
    x86_cpu_init_ap(core);
    __atomic_fetch_or(&g_ap_started_mask, 1u << core, __ATOMIC_RELEASE);
    rad_cpu_interrupts_enable();
    void (*entry)(uint32_t) = g_ap_worker_entries[core];
    if (entry) entry(core);
    rad_cpu_halt_forever();
}

extern "C" rad_status_t x86_cpu_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
    if (!entry || core == 0 || core >= MaxX86Cores) return RAD_STATUS_INVALID_ARGUMENT;
    if (core >= x86_cpu_detected_core_count()) return RAD_STATUS_NOT_SUPPORTED;
    if (__atomic_load_n(&g_ap_started_mask, __ATOMIC_ACQUIRE) & (1u << core)) return RAD_STATUS_OK;
    if (!g_lapic_address) return RAD_STATUS_NOT_SUPPORTED;

    const size_t trampoline_size = static_cast<size_t>(x86_ap_trampoline_end - x86_ap_trampoline_start);
    if (trampoline_size == 0 || trampoline_size > 4096u) return RAD_STATUS_NOT_SUPPORTED;
    auto *trampoline = reinterpret_cast<uint8_t *>(ApTrampolineAddress);
    memcpy(trampoline, x86_ap_trampoline_start, trampoline_size);
    *reinterpret_cast<uint32_t *>(trampoline + x86_ap_trampoline_pml4_offset) =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(x86_boot_pml4));
    *reinterpret_cast<uint32_t *>(trampoline + x86_ap_trampoline_core_offset) = core;
    *reinterpret_cast<uint64_t *>(trampoline + x86_ap_trampoline_stack_offset) =
        reinterpret_cast<uint64_t>(&g_ap_stacks[core][ApStackSize]);
    *reinterpret_cast<uint64_t *>(trampoline + x86_ap_trampoline_entry_offset) =
        reinterpret_cast<uint64_t>(x86_ap_entry);

    g_ap_worker_entries[core] = entry;
    g_ap_starting_core = core;
    lapic_write(LapicSvr, lapic_read(LapicSvr) | 0x100u);

    const uint8_t apic_id = static_cast<uint8_t>(apic_id_for_core(core));
    lapic_send_ipi(apic_id, 0x0000c500u);
    delay_pause(200000u);
    lapic_send_ipi(apic_id, 0x00008500u);
    delay_pause(200000u);
    lapic_send_ipi(apic_id, 0x00000600u | ApTrampolineVector);
    delay_pause(200000u);
    lapic_send_ipi(apic_id, 0x00000600u | ApTrampolineVector);

    for (uint32_t i = 0; i < 5000000u; ++i) {
        if (__atomic_load_n(&g_ap_started_mask, __ATOMIC_ACQUIRE) & (1u << core)) {
            rad_debug_marker("RAD_AP_START_OK");
            return RAD_STATUS_OK;
        }
        asm volatile("pause");
    }
    return RAD_STATUS_TIMEOUT;
}

// Arm the boot-stack guard page. The boot page tables (boot.S) map memory with 2 MiB
// pages, so we split the 2 MiB page covering the guard into 4 KiB pages via
// g_stack_guard_pt and leave only the guard page non-present. A stack overflow then
// takes a page fault on the guard (escalating to #DF, which is delivered on the IST
// stack) instead of silently scribbling over adjacent .bss. Must run after long mode
// is active and before the stack is used heavily.
extern "C" void x86_install_stack_guard(void) {
    const uint64_t guard = reinterpret_cast<uint64_t>(x86_boot_stack_guard);
    const uint64_t region = guard & ~0x1fffffull;           // 2 MiB-aligned base
    const uint64_t addr_mask = 0x000ffffffffff000ull;
    uint64_t *pml4 = reinterpret_cast<uint64_t*>(read_cr3() & addr_mask);
    uint64_t *pdpt = reinterpret_cast<uint64_t*>(pml4[(guard >> 39) & 0x1ffu] & addr_mask);
    uint64_t *pd = reinterpret_cast<uint64_t*>(pdpt[(guard >> 30) & 0x1ffu] & addr_mask);
    const uint32_t pd_index = (guard >> 21) & 0x1ffu;
    for (uint32_t i = 0; i < 512u; ++i) {
        const uint64_t page = region + static_cast<uint64_t>(i) * 4096u;
        const bool in_guard = (page >= guard) && (page < guard + kBootStackGuardBytes);
        g_stack_guard_pt[i] = in_guard ? 0ull : (page | 0x3ull); // non-present guard | present|write
    }
    pd[pd_index] = (reinterpret_cast<uint64_t>(g_stack_guard_pt) & addr_mask) | 0x3ull;
    write_cr3(read_cr3()); // flush the TLB for the remapped region
    rad_debug_marker("RAD_X86_STACK_GUARD_OK");
}

#if defined(RAD_X86_STACK_GUARD_SELFTEST)
// Opt-in validation of the guard page + IST report path: unbounded recursion that
// touches an 8 KiB frame each level until it overflows the boot stack into the guard
// page. Expected result: a #DF on the IST stack reporting RAD_X86_STACK_GUARD_OVERFLOW,
// then a clean halt -- never silent corruption or a triple fault. Build with
// -DRAD_X86_STACK_GUARD_SELFTEST to exercise it.
volatile uint64_t g_guard_selftest_sink = 0;
extern "C" void x86_stack_guard_selftest(void) {
    volatile char buf[8192];
    buf[0] = 1;
    buf[sizeof(buf) - 1] = 1;
    x86_stack_guard_selftest();
    g_guard_selftest_sink += static_cast<uint64_t>(buf[0]); // use after call: no tail-call
}
#endif

extern "C" void x86_cpu_init(uint64_t kernel_stack_top) {
    memset(g_gdt, 0, sizeof(g_gdt));
    memset(g_tss, 0, sizeof(g_tss));
    memset(g_idt, 0, sizeof(g_idt));
    prepare_gdt_tss_for_core(0, kernel_stack_top);

    for (uint16_t i = 0; i < 32; ++i) {
        set_idt_gate(static_cast<uint8_t>(i), x86_exception_no_error_entry, 0x8eu);
    }
    set_idt_gate(8, x86_double_fault_entry, 0x8eu);
    set_idt_gate(10, x86_invalid_tss_entry, 0x8eu);
    set_idt_gate(6, x86_invalid_opcode_entry, 0x8eu);
    set_idt_gate(7, x86_device_not_available_entry, 0x8eu);
    set_idt_gate(11, x86_segment_not_present_entry, 0x8eu);
    set_idt_gate(12, x86_stack_segment_entry, 0x8eu);
    set_idt_gate(13, x86_general_protection_entry, 0x8eu);
    set_idt_gate(14, x86_page_fault_entry, 0x8eu);
    set_idt_gate(16, x86_floating_point_entry, 0x8eu);
    set_idt_gate(17, x86_alignment_check_entry, 0x8eu);
    set_idt_gate(19, x86_simd_floating_point_entry, 0x8eu);
    set_idt_gate(21, x86_control_protection_entry, 0x8eu);
    // Deliver the fatal, stack-sensitive traps on the IST stack (TSS.ist[0]) so they
    // survive a boot-stack overflow: a guard-page hit faults (#PF) while the stack is
    // exhausted, which escalates to #DF -- and #DF/#GP must land on a known-good stack
    // to be reported instead of triple-faulting the machine.
    g_idt[8].ist = 1;   // #DF
    g_idt[13].ist = 1;  // #GP
    set_idt_gate(0x80, x86_int80_entry, 0xeeu);
    set_idt_gate(LapicTimerVector, x86_lapic_timer_entry, 0x8eu);
    for (uint8_t i = 0; i < 16; ++i) {
        set_idt_gate(static_cast<uint8_t>(PicMasterVectorBase + i), g_irq_entries[i], 0x8eu);
    }

    load_gdt_tss_for_core(0);
    load_shared_idt();
    x86_install_stack_guard();
#if defined(RAD_X86_STACK_GUARD_SELFTEST)
    x86_stack_guard_selftest(); // opt-in: prove the guard catches overflow (see above)
#endif
    enable_sse();
    pic_remap_and_mask();
    configure_syscall_msrs();
    if (g_lapic_address) lapic_write(LapicSvr, lapic_read(LapicSvr) | 0x100u);
    x86_lapic_timer_start_current_core();

    rad_debug_marker("RAD_X86_CPU_OK");
    rad_debug_marker("RAD_IRQ_CORE_OK");
    rad_debug_marker("RAD_PIC_REMAP_OK");
}

extern "C" void x86_cpu_init_ap(uint32_t core) {
    if (core >= MaxX86Cores) rad_cpu_halt_forever();
    prepare_gdt_tss_for_core(core, reinterpret_cast<uint64_t>(&g_ap_stacks[core][ApStackSize]));
    load_gdt_tss_for_core(core);
    load_shared_idt();
    enable_sse();
    configure_syscall_msrs();
    if (g_lapic_address) lapic_write(LapicSvr, lapic_read(LapicSvr) | 0x100u);
    x86_lapic_timer_start_current_core();
}

extern "C" void x86_cpu_set_kernel_stack_top(uint64_t kernel_stack_top) {
    const uint32_t core = x86_cpu_current_core();
    if (kernel_stack_top && core < MaxX86Cores) g_tss[core].rsp0 = kernel_stack_top;
}

extern "C" int x86_cpu_self_test(void) {
    const intptr_t pid = x86_test_int80_getpid();
    if (pid == 0) {
        rad_debug_marker("RAD_INT80_OK");
        return 1;
    }
    rad_debug_marker("RAD_INT80_FAIL");
    return 0;
}

extern "C" int x86_context_self_test(void) {
    memset(&g_context_probe_main, 0, sizeof(g_context_probe_main));
    memset(&g_context_probe_task, 0, sizeof(g_context_probe_task));
    g_context_probe_stage = 0;

    uintptr_t stack_top = reinterpret_cast<uintptr_t>(g_context_probe_stack) + sizeof(g_context_probe_stack);
    stack_top = align_down_uintptr(stack_top, 16u) - 8u;
    uint64_t *stack = reinterpret_cast<uint64_t *>(stack_top);
    *--stack = reinterpret_cast<uint64_t>(x86_context_probe_entry);
    g_context_probe_task.rsp = reinterpret_cast<uint64_t>(stack);

    x86_context_switch(&g_context_probe_main, &g_context_probe_task);
    if (g_context_probe_stage != 1u) {
        rad_debug_marker("RAD_CONTEXT_SWITCH_FAIL");
        return 0;
    }

    x86_context_switch(&g_context_probe_main, &g_context_probe_task);
    if (g_context_probe_stage != 2u) {
        rad_debug_marker("RAD_CONTEXT_SWITCH_FAIL");
        return 0;
    }

    rad_debug_marker("RAD_CONTEXT_SWITCH_OK");
    return 1;
}

extern "C" int rad_arch_task_context_supported(void) {
    return 1;
}

extern "C" void rad_arch_task_context_init(uintptr_t *context, void *stack_top, void (*entry)(void)) {
    if (!context || !stack_top || !entry) return;
    memset(context, 0, sizeof(uintptr_t) * 8u);
    uintptr_t top = align_down_uintptr(reinterpret_cast<uintptr_t>(stack_top), 16u) - 8u;
    auto *stack = reinterpret_cast<uint64_t *>(top);
    *--stack = reinterpret_cast<uint64_t>(entry);
    context[0] = reinterpret_cast<uintptr_t>(stack);
}

extern "C" void rad_arch_task_context_switch(uintptr_t *old_context, uintptr_t *new_context) {
    x86_context_switch(old_context, new_context);
}
