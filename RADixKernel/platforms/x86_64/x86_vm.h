#ifndef RAD_X86_64_GRUB_SLINT_VM_H
#define RAD_X86_64_GRUB_SLINT_VM_H

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>

/** Summary of the x86_64 early virtual-memory allocator and address-space state. */
typedef struct x86_vm_summary {
    /** Usable physical memory discovered from the boot memory map. */
    uint64_t usable_bytes;
    /** Number of page frames tracked by the allocator. */
    uint64_t tracked_pages;
    /** Number of currently free page frames. */
    uint64_t free_pages;
    /** Number of reserved page frames. */
    uint64_t reserved_pages;
    /** Physical start address of the loaded kernel image. */
    uint64_t kernel_start;
    /** Physical end address of the loaded kernel image. */
    uint64_t kernel_end;
    /** Non-zero once the page allocator is initialized. */
    int page_allocator_ready;
    /** Non-zero once per-process address-space support is initialized. */
    int user_address_space_ready;
} x86_vm_summary_t;

/** x86_64 process address space descriptor owned by the RADix process layer. */
typedef struct x86_address_space {
    /** Physical address of the top-level PML4 table. */
    uint64_t pml4;
    /** Physical pages owned by this address space and released at teardown. */
    uint64_t owned_pages[16384];
    /** Number of valid entries in owned_pages. */
    size_t owned_page_count;
} x86_address_space_t;

extern "C" void x86_vm_init(const rad_boot_info_t *boot, x86_vm_summary_t *summary);
extern "C" uint64_t x86_vm_alloc_page(void);
extern "C" int x86_vm_retain_page(uint64_t physical_address);
extern "C" void x86_vm_free_page(uint64_t physical_address);
extern "C" int x86_vm_create_address_space(x86_address_space_t *space);
extern "C" int x86_vm_map_user_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable);
extern "C" int x86_vm_map_shared_page(x86_address_space_t *space, uint64_t virtual_address, uint64_t physical_address, int writable);
extern "C" int x86_vm_map_kernel_mmio(uint64_t physical_address, uint64_t size);
extern "C" int x86_vm_clone_cow(x86_address_space_t *child, x86_address_space_t *parent);
extern "C" int x86_vm_handle_page_fault(uint64_t fault_address, uint64_t error_code);
extern "C" void x86_vm_destroy_address_space(x86_address_space_t *space);
extern "C" void x86_vm_activate_address_space(const x86_address_space_t *space);
extern "C" void x86_vm_activate_kernel_address_space(void);
extern "C" int x86_vm_validate_user_range(uint64_t virtual_address, uint64_t size, int write);
extern "C" int x86_vm_address_space_isolated(const x86_address_space_t *space);
extern "C" int x86_vm_self_test(void);

#endif
