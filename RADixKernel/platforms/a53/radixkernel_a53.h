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
} rad_a53_capabilities_t;

/** @brief Record normalized boot state discovered by A53 assembly entry. */
void rad_a53_note_boot_normalized(uint32_t boot_core, uintptr_t boot_argument, uint32_t secondary_cores_parked);
/** @brief Initialize portable A53 execution capabilities and emit smoke markers. */
rad_status_t rad_a53_platform_init(void);
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
