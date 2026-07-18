/**
 * @file radboot.h
 * @brief Helpers for constructing rad_boot_info_t records during platform handoff.
 */

#ifndef RADBOOT_H
#define RADBOOT_H

#include <radixkernel/radixkernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize a boot info record with common backend, board, console, and SD mode strings.
void radboot_prepare_default(rad_boot_info_t *boot, const char *backend, const char *board, const char *console, const char *sd_mode);
/// Append a memory region to a boot info record when capacity remains.
void radboot_add_memory_region(rad_boot_info_t *boot, const char *name, uint64_t base, uint64_t size, uint32_t type, uint32_t flags);
/// Append a key/value boot argument to a boot info record when capacity remains.
void radboot_add_arg(rad_boot_info_t *boot, const char *key, const char *value);

/// Fill boot defaults for RP235x targets.
void radboot_prepare_rp235x(rad_boot_info_t *boot, const char *board, const char *sd_mode);
/// Fill boot defaults for Circle-backed Raspberry Pi targets.
void radboot_prepare_circle(rad_boot_info_t *boot, const char *board, const char *sd_mode);
/// Fill boot defaults for the standalone RAD-owned BCM283x Raspberry Pi backend.
void radboot_prepare_bcm283x(rad_boot_info_t *boot, const char *board, const char *sd_mode);
/// Initialize a RADPx boot handoff record with Pi Zero 2 W defaults.
void radboot_prepare_pi_handoff(rad_boot_handoff_t *handoff, const char *payload_name, uintptr_t payload_base, uintptr_t payload_size, uintptr_t payload_entry);
/// Validate a RADPx boot handoff record before entering the standalone kernel.
rad_status_t radboot_validate_handoff(const rad_boot_handoff_t *handoff);

#ifdef __cplusplus
}
#endif

#endif
