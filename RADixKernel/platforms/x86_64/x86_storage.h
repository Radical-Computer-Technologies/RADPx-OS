#ifndef RAD_X86_64_GRUB_SLINT_STORAGE_H
#define RAD_X86_64_GRUB_SLINT_STORAGE_H

#include <stddef.h>
#include <stdint.h>

/** Summary of x86_64 PCI/virtio storage and network device discovery. */
typedef struct x86_storage_summary {
    /** Number of PCI devices discovered during the probe. */
    uint32_t pci_devices;
    /** Number of virtio block devices discovered. */
    uint32_t virtio_block_devices;
    /** Number of block devices registered with the RADPx block layer. */
    uint32_t registered_block_devices;
    /** Number of virtio network devices discovered. */
    uint32_t virtio_net_devices;
    /** Number of network devices registered with the RADPx network layer. */
    uint32_t registered_net_devices;
    /** PCI bus containing the primary virtio block device. */
    uint8_t virtio_bus;
    /** PCI slot containing the primary virtio block device. */
    uint8_t virtio_slot;
    /** PCI function containing the primary virtio block device. */
    uint8_t virtio_function;
    /** Reserved for ABI alignment and future flags. */
    uint8_t reserved;
} x86_storage_summary_t;

extern "C" void x86_storage_probe(x86_storage_summary_t *summary);
extern "C" int x86_storage_self_test(void);
extern "C" int x86_network_self_test(void);
extern "C" void x86_virtio_input_poll(void);

#endif
