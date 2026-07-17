#ifndef RADIXKERNEL_PLATFORMS_A53_H
#define RADIXKERNEL_PLATFORMS_A53_H

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Portable Cortex-A53/ARMv8 execution capability snapshot. */
typedef struct rad_a53_capabilities {
    uint32_t size; ///< Structure size for ABI/version checks.
    uint32_t boot_normalized; ///< Nonzero when the entry path masked IRQs, selected core 0, and set a clean stack.
    uint32_t secondary_cores_parked; ///< Nonzero when secondary cores were parked before kernel entry.
    uint32_t exception_vectors_ready; ///< Nonzero when the AArch64 exception-vector contract is installed or modeled.
    uint32_t svc_ready; ///< Nonzero when SVC/syscall dispatch is available through the A53 platform layer.
    uint32_t user_copy_ready; ///< Nonzero when user pointer validation/copy helpers are available.
    uint32_t fork_ready; ///< Nonzero when process fork hooks are registered.
    uint32_t exec_ready; ///< Nonzero when exec path integration is available.
    uint32_t cow_ready; ///< Nonzero when copy-on-write tracking is available.
    uintptr_t boot_argument; ///< Raw boot argument preserved from x0 by the entry path.
    uint32_t boot_core; ///< Core ID that entered the primary kernel path.
    uintptr_t user_base; ///< Lowest user virtual address managed by the A53 layer.
    uintptr_t user_limit; ///< Exclusive upper user virtual address limit managed by the A53 layer.
    uint32_t page_size; ///< Translation/page granule size used by the current A53 layer.
    uint32_t mmu_ready; ///< Nonzero when hardware page tables are built or modeled.
    uint32_t ttbr0_user_ready; ///< Nonzero when user address-space roots are available.
    uint32_t ttbr1_kernel_ready; ///< Nonzero when the kernel translation root is available.
} rad_a53_capabilities_t;

/** @brief A53 VM initialization summary. */
typedef struct rad_a53_vm_summary {
    uint32_t size; ///< Structure size for ABI/version checks.
    uintptr_t kernel_table; ///< Kernel translation table root.
    uintptr_t active_table; ///< Active TTBR0-compatible translation root.
    uintptr_t usable_base; ///< First managed physical page.
    uintptr_t usable_limit; ///< Exclusive end of managed physical memory.
    uint32_t tracked_pages; ///< Number of tracked 4 KiB pages.
    uint32_t free_pages; ///< Number of currently free pages.
    uint32_t reserved_pages; ///< Number of currently reserved pages.
    uint32_t hardware_mmu_enabled; ///< Nonzero when SCTLR_EL1.M is enabled by this layer.
} rad_a53_vm_summary_t;

/** @brief A53 process address-space root and owned physical pages. */
typedef struct rad_a53_address_space {
    uintptr_t ttbr0; ///< TTBR0-compatible translation table root for user mappings.
    uintptr_t owned_pages[8192]; ///< Physical pages owned or retained by this address space.
    size_t owned_page_count; ///< Number of valid entries in owned_pages.
    uintptr_t next_mmap; ///< Next user virtual address candidate for mmap-style allocations.
} rad_a53_address_space_t;

/** @brief AArch64 lower-EL trap frame used for syscall/fork handoff.
 *
 * The FP/SIMD block is appended after the general-purpose fields so their
 * offsets (hardcoded in boot.S) stay stable: q[] at +288, fpsr +800, fpcr +808,
 * total 816 bytes. User processes may use FP/NEON (ports such as ncurses do);
 * kernel code is -mgeneral-regs-only, so the exception paths in boot.S are the
 * only place this state is saved and restored. */
typedef struct rad_a53_trap_frame {
    uint64_t x[31]; ///< General-purpose registers x0-x30 captured at exception entry.
    uintptr_t sp_el0; ///< Lower-exception-level stack pointer.
    uintptr_t elr_el1; ///< Exception link register used to resume EL0.
    uint64_t spsr_el1; ///< Saved processor state for the interrupted EL0 context.
    uint64_t esr_el1; ///< Exception syndrome register value.
    uintptr_t far_el1; ///< Fault address register value for abort handling.
    uint64_t q[64]; ///< FP/SIMD registers q0-q31 (two 64-bit halves each).
    uint64_t fpsr; ///< Floating-point status register.
    uint64_t fpcr; ///< Floating-point control register.
} rad_a53_trap_frame_t;

/** @brief Record normalized boot state discovered by A53 assembly entry. */
void rad_a53_note_boot_normalized(uint32_t boot_core, uintptr_t boot_argument, uint32_t secondary_cores_parked);
/** @brief Initialize portable A53 execution capabilities and emit smoke markers. */
rad_status_t rad_a53_platform_init(void);
/** @brief Initialize A53 translation tables and physical page accounting. */
rad_status_t rad_a53_vm_init(const rad_boot_info_t *boot, rad_a53_vm_summary_t *summary);
/** @brief Allocate one managed physical page. */
uintptr_t rad_a53_vm_alloc_page(void);
/** @brief Retain one managed physical page. */
int rad_a53_vm_retain_page(uintptr_t physical_address);
/** @brief Release one managed physical page. */
void rad_a53_vm_free_page(uintptr_t physical_address);
/** @brief Create a per-process A53 address space. */
rad_status_t rad_a53_create_address_space(rad_a53_address_space_t *space);
/** @brief Destroy a per-process A53 address space. */
void rad_a53_destroy_address_space(rad_a53_address_space_t *space);
/** @brief Activate a process address space for user memory access. */
rad_status_t rad_a53_activate_address_space(rad_a53_address_space_t *space);
/** @brief Return to the kernel translation root. */
void rad_a53_activate_kernel_address_space(void);
/** @brief Map a user page into an A53 address space. */
rad_status_t rad_a53_map_user_page(rad_a53_address_space_t *space, uintptr_t virtual_address, uintptr_t physical_address, int writable);
/** @brief Clone a parent address space into a child using copy-on-write. */
rad_status_t rad_a53_clone_cow(rad_a53_address_space_t *child, rad_a53_address_space_t *parent);
/** @brief Handle an AArch64 data abort as a possible COW fault. */
int rad_a53_handle_data_abort(uintptr_t fault_address, uint64_t esr_el1);
/** @brief Validate a user virtual range against the active A53 address space. */
int rad_a53_validate_user_range(uintptr_t virtual_address, size_t size, int write);
/** @brief Copy bytes from active user memory. */
rad_status_t rad_a53_copy_from_user(void *dst, uintptr_t src, size_t size);
/** @brief Copy bytes to active user memory. */
rad_status_t rad_a53_copy_to_user(uintptr_t dst, const void *src, size_t size);
/** @brief Copy a NUL-terminated string from active user memory. */
rad_status_t rad_a53_copy_string_from_user(char *dst, size_t dst_size, uintptr_t src);
/** @brief Dispatch a syscall captured in an AArch64 trap frame. */
intptr_t rad_a53_syscall_dispatch_frame(rad_a53_trap_frame_t *frame);
/** @brief Dispatch a syscall using the AArch64 x8/x0-x5 ABI contract. */
intptr_t rad_a53_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);
/** @brief Spawn a user process through the A53 process layer. */
rad_status_t rad_a53_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out);
/** @brief Spawn a user process and bind stdin/stdout/stderr to a device or PTY path. */
rad_status_t rad_a53_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out);
/** @brief Run a user init process through the A53 process layer. */
int rad_a53_user_run_init(const char *path);
/** @brief Dispatch an AArch64 exception frame saved by the platform vector stubs. */
uintptr_t rad_a53_exception_dispatch(rad_a53_trap_frame_t *frame);
/** @brief Enter or resume an AArch64 EL0 context. */
void rad_a53_enter_user_context(void *context);
/** @brief Register the A53 process architecture operations with the kernel core. */
rad_status_t rad_a53_process_arch_init(void);
/** @brief Copy the current A53 capability snapshot to the caller. */
rad_status_t rad_a53_get_capabilities(rad_a53_capabilities_t *capabilities);
/** @brief Run the A53 process fork/exec-mark/wait smoke path. */
rad_status_t rad_a53_process_self_test(void);
/** @brief Run the A53 copy-on-write page isolation model self-test. */
int rad_a53_vm_self_test(void);

#ifdef __cplusplus
}
#endif

#endif
