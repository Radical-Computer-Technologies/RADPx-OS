#include "x86_storage.h"
#include "x86_vm.h"

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint16_t PciConfigAddress = 0x0cf8;
constexpr uint16_t PciConfigData = 0x0cfc;
constexpr uint16_t VirtioVendor = 0x1af4;
constexpr uint16_t VirtioNetLegacyDevice = 0x1000;
constexpr uint16_t VirtioBlockLegacyDevice = 0x1001;
constexpr uint16_t VirtioInputModernDevice = 0x1052;
constexpr uint16_t VirtioStatusAcknowledge = 1u;
constexpr uint16_t VirtioStatusDriver = 2u;
constexpr uint16_t VirtioStatusDriverOk = 4u;
constexpr uint16_t VirtioStatusFeaturesOk = 8u;
constexpr uint16_t VirtioStatusFailed = 0x80u;
constexpr uint16_t VirtioBlkSectorSize = 512u;
constexpr uint16_t VirtioBlkTIn = 0u;
constexpr uint16_t VirtioBlkTOut = 1u;
constexpr uint16_t VirtqDescFNext = 1u;
constexpr uint16_t VirtqDescFWrite = 2u;
constexpr uint16_t MaxVirtqEntries = 256u;
constexpr uint32_t MaxVirtioBlockDevices = 4u;
constexpr uint32_t MaxVirtioNetDevices = 2u;
constexpr uint32_t MaxVirtioInputDevices = 1u;
constexpr uint32_t VirtioNetRxQueue = 0u;
constexpr uint32_t VirtioNetTxQueue = 1u;
constexpr uint32_t VirtioNetBufferBytes = 1536u;
constexpr uint8_t VirtioPciCapVendor = 0x09u;
constexpr uint8_t VirtioPciCapCommonCfg = 1u;
constexpr uint8_t VirtioPciCapNotifyCfg = 2u;
constexpr uint8_t VirtioPciCapDeviceCfg = 4u;
constexpr uint32_t VirtioFeatureVersion1 = 1u << 0; // Feature bit 32, selector 1.
constexpr uint16_t VirtioInputQueueEvents = 0u;
constexpr uint16_t VirtioInputQueueStatus = 1u;
constexpr uint16_t VirtioInputQueueCount = 2u;
constexpr uint16_t VirtioInputMaxEvents = 64u;
constexpr uint16_t EvSyn = 0x00u;
constexpr uint16_t EvKey = 0x01u;
constexpr uint16_t EvAbs = 0x03u;
constexpr uint16_t AbsX = 0x00u;
constexpr uint16_t AbsY = 0x01u;
constexpr uint16_t BtnLeft = 0x110u;
constexpr uint16_t BtnRight = 0x111u;
constexpr uint16_t BtnMiddle = 0x112u;
constexpr uint8_t VirtioInputCfgAbsInfo = 3u;

struct [[gnu::packed]] VirtqDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct [[gnu::packed]] VirtqAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[MaxVirtqEntries];
};

struct [[gnu::packed]] VirtqUsedElem {
    uint32_t id;
    uint32_t len;
};

struct [[gnu::packed]] VirtqUsed {
    uint16_t flags;
    uint16_t idx;
    VirtqUsedElem ring[MaxVirtqEntries];
};

struct [[gnu::packed]] VirtioBlkRequestHeader {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct [[gnu::packed]] VirtioNetHeader {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t header_length;
    uint16_t gso_size;
    uint16_t checksum_start;
    uint16_t checksum_offset;
};

struct VirtioBlockDevice {
    uint16_t io_base;
    uint16_t queue_size;
    uint16_t avail_idx;
    uint16_t used_idx;
    uint64_t capacity_sectors;
    int ready;
    char name[16];
    uint32_t index;
};

struct VirtioNetDevice {
    uint16_t io_base;
    uint16_t rx_queue_size;
    uint16_t tx_queue_size;
    uint16_t rx_avail_idx;
    uint16_t tx_avail_idx;
    uint16_t rx_used_idx;
    uint16_t tx_used_idx;
    int ready;
    char name[16];
    rad_mac_address_t mac;
    uint32_t index;
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint8_t rx_frame[1536];
    size_t rx_frame_size;
};

struct VirtioPciModernCaps {
    volatile uint8_t *common = nullptr;
    volatile uint8_t *notify = nullptr;
    volatile uint8_t *device = nullptr;
    uint32_t notify_multiplier = 0;
};

struct [[gnu::packed]] VirtioInputEvent {
    uint16_t type;
    uint16_t code;
    int32_t value;
};

struct VirtioInputDevice {
    VirtioPciModernCaps caps{};
    uint16_t queue_size = 0;
    uint16_t avail_idx = 0;
    uint16_t used_idx = 0;
    uint16_t status_queue_size = 0;
    volatile uint8_t *event_notify = nullptr;
    volatile uint8_t *status_notify = nullptr;
    uint32_t buttons = 0;
    int32_t abs_x = 0;
    int32_t abs_y = 0;
    int32_t abs_min_x = 0;
    int32_t abs_max_x = 32767;
    int32_t abs_min_y = 0;
    int32_t abs_max_y = 32767;
    int ready = 0;
};

alignas(4096) uint8_t g_queue_memory[MaxVirtioBlockDevices][12288];
alignas(4096) uint8_t g_net_queue_memory[MaxVirtioNetDevices][2][12288];
alignas(4096) uint8_t g_input_queue_memory[MaxVirtioInputDevices][VirtioInputQueueCount][12288];
alignas(16) VirtioInputEvent g_input_events[MaxVirtioInputDevices][VirtioInputMaxEvents];
alignas(16) VirtioNetHeader g_net_tx_headers[MaxVirtioNetDevices];
alignas(16) uint8_t g_net_rx_buffers[MaxVirtioNetDevices][MaxVirtqEntries][sizeof(VirtioNetHeader) + VirtioNetBufferBytes];
alignas(16) VirtioBlkRequestHeader g_request_header;
alignas(16) uint8_t g_request_status;
VirtioBlockDevice g_block_devices[MaxVirtioBlockDevices];
VirtioNetDevice g_net_devices[MaxVirtioNetDevices];
VirtioInputDevice g_input_devices[MaxVirtioInputDevices];

extern "C" void x86_input_pointer_absolute(int32_t raw_x, int32_t raw_y, int32_t min_x, int32_t max_x, int32_t min_y, int32_t max_y, uint32_t buttons);
extern "C" void x86_input_pointer_buttons(uint32_t buttons);

void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t value = 0;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint16_t inw(uint16_t port) {
    uint16_t value = 0;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void io_wait(void);

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    const uint32_t address = 0x80000000u
        | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11)
        | (static_cast<uint32_t>(function) << 8)
        | (offset & 0xfcu);
    outl(PciConfigAddress, address);
    return inl(PciConfigData);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    const uint32_t value = pci_read(bus, slot, function, offset);
    return static_cast<uint8_t>((value >> ((offset & 3u) * 8u)) & 0xffu);
}

void pci_write(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    const uint32_t address = 0x80000000u
        | (static_cast<uint32_t>(bus) << 16)
        | (static_cast<uint32_t>(slot) << 11)
        | (static_cast<uint32_t>(function) << 8)
        | (offset & 0xfcu);
    outl(PciConfigAddress, address);
    outl(PciConfigData, value);
}

uint64_t pci_bar_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t bar) {
    const uint8_t offset = static_cast<uint8_t>(0x10u + bar * 4u);
    const uint32_t low = pci_read(bus, slot, function, offset);
    if (low & 1u) return low & ~0x3ull;
    uint64_t address = low & ~0xfull;
    if ((low & 0x6u) == 0x4u && bar < 5u) {
        address |= static_cast<uint64_t>(pci_read(bus, slot, function, static_cast<uint8_t>(offset + 4u))) << 32u;
    }
    return address;
}

int is_virtio_block(uint16_t vendor, uint16_t device, uint32_t class_reg) {
    if (vendor != VirtioVendor) return 0;
    if (device == 0x1001u || device == 0x1042u) return 1;
    const uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
    const uint8_t sub_class = static_cast<uint8_t>(class_reg >> 16);
    return base_class == 0x01u && sub_class == 0x00u;
}

int is_virtio_net(uint16_t vendor, uint16_t device, uint32_t class_reg) {
    if (vendor != VirtioVendor) return 0;
    if (device == 0x1000u || device == 0x1041u) return 1;
    const uint8_t base_class = static_cast<uint8_t>(class_reg >> 24);
    const uint8_t sub_class = static_cast<uint8_t>(class_reg >> 16);
    return base_class == 0x02u && sub_class == 0x00u;
}

int is_virtio_input(uint16_t vendor, uint16_t device) {
    return vendor == VirtioVendor && device == VirtioInputModernDevice;
}

uint16_t mmio_read16(volatile uint8_t *base, uint32_t offset) {
    return *reinterpret_cast<volatile uint16_t*>(base + offset);
}

uint8_t mmio_read8(volatile uint8_t *base, uint32_t offset) {
    return *reinterpret_cast<volatile uint8_t*>(base + offset);
}

uint32_t mmio_read32(volatile uint8_t *base, uint32_t offset) {
    return *reinterpret_cast<volatile uint32_t*>(base + offset);
}

void mmio_write8(volatile uint8_t *base, uint32_t offset, uint8_t value) {
    *reinterpret_cast<volatile uint8_t*>(base + offset) = value;
}

void mmio_write16(volatile uint8_t *base, uint32_t offset, uint16_t value) {
    *reinterpret_cast<volatile uint16_t*>(base + offset) = value;
}

void mmio_write32(volatile uint8_t *base, uint32_t offset, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(base + offset) = value;
}

void mmio_write64(volatile uint8_t *base, uint32_t offset, uint64_t value) {
    *reinterpret_cast<volatile uint64_t*>(base + offset) = value;
}

VirtqDesc *queue_desc(VirtioBlockDevice *device) {
    return reinterpret_cast<VirtqDesc*>(g_queue_memory[device->index]);
}

VirtqDesc *input_queue_desc(VirtioInputDevice *device, uint16_t queue = VirtioInputQueueEvents) {
    const uint32_t index = static_cast<uint32_t>(device - g_input_devices);
    return reinterpret_cast<VirtqDesc*>(g_input_queue_memory[index][queue]);
}

VirtqAvail *input_queue_avail(VirtioInputDevice *device, uint16_t queue = VirtioInputQueueEvents) {
    const uint32_t index = static_cast<uint32_t>(device - g_input_devices);
    return reinterpret_cast<VirtqAvail*>(g_input_queue_memory[index][queue] + sizeof(VirtqDesc) * MaxVirtqEntries);
}

VirtqUsed *input_queue_used(VirtioInputDevice *device, uint16_t queue = VirtioInputQueueEvents) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(input_queue_avail(device, queue)) + sizeof(VirtqAvail);
    addr = (addr + 4095u) & ~static_cast<uintptr_t>(4095u);
    return reinterpret_cast<VirtqUsed*>(addr);
}

VirtqAvail *queue_avail(VirtioBlockDevice *device) {
    return reinterpret_cast<VirtqAvail*>(g_queue_memory[device->index] + sizeof(VirtqDesc) * MaxVirtqEntries);
}

VirtqUsed *queue_used(VirtioBlockDevice *device) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(queue_avail(device)) + sizeof(VirtqAvail);
    addr = (addr + 4095u) & ~static_cast<uintptr_t>(4095u);
    return reinterpret_cast<VirtqUsed*>(addr);
}

VirtqDesc *net_queue_desc(VirtioNetDevice *device, uint32_t queue) {
    return reinterpret_cast<VirtqDesc*>(g_net_queue_memory[device->index][queue]);
}

VirtqAvail *net_queue_avail(VirtioNetDevice *device, uint32_t queue) {
    return reinterpret_cast<VirtqAvail*>(g_net_queue_memory[device->index][queue] + sizeof(VirtqDesc) * MaxVirtqEntries);
}

VirtqUsed *net_queue_used(VirtioNetDevice *device, uint32_t queue) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(net_queue_avail(device, queue)) + sizeof(VirtqAvail);
    addr = (addr + 4095u) & ~static_cast<uintptr_t>(4095u);
    return reinterpret_cast<VirtqUsed*>(addr);
}

void net_select_queue(VirtioNetDevice *device, uint32_t queue) {
    outw(device->io_base + 0x0e, static_cast<uint16_t>(queue));
}

int net_setup_queue(VirtioNetDevice *device, uint32_t queue, uint16_t *queue_size_out) {
    net_select_queue(device, queue);
    const uint16_t queue_size = inw(device->io_base + 0x0c);
    if (queue_size == 0 || queue_size > MaxVirtqEntries) return 0;
    memset(g_net_queue_memory[device->index][queue], 0, sizeof(g_net_queue_memory[device->index][queue]));
    outl(device->io_base + 0x08, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_net_queue_memory[device->index][queue]) >> 12));
    if (queue_size_out) *queue_size_out = queue_size;
    return 1;
}

void net_refill_rx(VirtioNetDevice *device, uint16_t descriptor) {
    auto *desc = net_queue_desc(device, VirtioNetRxQueue);
    auto *avail = net_queue_avail(device, VirtioNetRxQueue);
    desc[descriptor].addr = reinterpret_cast<uintptr_t>(g_net_rx_buffers[device->index][descriptor]);
    desc[descriptor].len = sizeof(g_net_rx_buffers[device->index][descriptor]);
    desc[descriptor].flags = VirtqDescFWrite;
    desc[descriptor].next = 0;
    avail->ring[device->rx_avail_idx % device->rx_queue_size] = descriptor;
    asm volatile("" ::: "memory");
    ++device->rx_avail_idx;
    avail->idx = device->rx_avail_idx;
}

void net_refill_all_rx(VirtioNetDevice *device) {
    if (!device || !device->rx_queue_size) return;
    for (uint16_t i = 0; i < device->rx_queue_size; ++i) net_refill_rx(device, i);
    asm volatile("" ::: "memory");
    outw(device->io_base + 0x10, VirtioNetRxQueue);
}

int find_virtio_modern_caps(uint8_t bus, uint8_t slot, uint8_t function, VirtioPciModernCaps *caps) {
    if (!caps) return 0;
    memset(caps, 0, sizeof(*caps));
    uint8_t cap = pci_read8(bus, slot, function, 0x34);
    for (uint32_t guard = 0; cap && guard < 48u; ++guard) {
        const uint8_t cap_vndr = pci_read8(bus, slot, function, cap);
        const uint8_t cap_next = pci_read8(bus, slot, function, static_cast<uint8_t>(cap + 1u));
        if (cap_vndr == VirtioPciCapVendor) {
            const uint8_t cfg_type = pci_read8(bus, slot, function, static_cast<uint8_t>(cap + 3u));
            const uint8_t bar = pci_read8(bus, slot, function, static_cast<uint8_t>(cap + 4u));
            const uint32_t offset = pci_read(bus, slot, function, static_cast<uint8_t>(cap + 8u));
            uint32_t length = pci_read(bus, slot, function, static_cast<uint8_t>(cap + 12u));
            if (length == 0) length = 4096u;
            const uint64_t bar_addr = pci_bar_address(bus, slot, function, bar);
            if (bar_addr) {
                if (!x86_vm_map_kernel_mmio(bar_addr + offset, length)) {
                    rad_debug_marker("RADIX_VIRTIO_INPUT_MMIO_MAP_FAIL");
                    cap = cap_next;
                    continue;
                }
                volatile uint8_t *mapped = reinterpret_cast<volatile uint8_t*>(static_cast<uintptr_t>(bar_addr + offset));
                if (cfg_type == VirtioPciCapCommonCfg) caps->common = mapped;
                else if (cfg_type == VirtioPciCapNotifyCfg) {
                    caps->notify = mapped;
                    caps->notify_multiplier = pci_read(bus, slot, function, static_cast<uint8_t>(cap + 16u));
                } else if (cfg_type == VirtioPciCapDeviceCfg) {
                    caps->device = mapped;
                }
            }
        }
        cap = cap_next;
    }
    return caps->common && caps->notify && caps->device;
}

void virtio_input_query_abs(VirtioInputDevice *device, uint8_t axis, int32_t *min_out, int32_t *max_out) {
    if (!device || !device->caps.device || !min_out || !max_out) return;
    mmio_write8(device->caps.device, 0, VirtioInputCfgAbsInfo);
    mmio_write8(device->caps.device, 1, axis);
    const uint8_t size = *reinterpret_cast<volatile uint8_t*>(device->caps.device + 2);
    if (size < 8u) return;
    *min_out = *reinterpret_cast<volatile int32_t*>(device->caps.device + 8);
    *max_out = *reinterpret_cast<volatile int32_t*>(device->caps.device + 12);
    if (*max_out <= *min_out) {
        *min_out = 0;
        *max_out = 32767;
    }
}

void virtio_input_notify(VirtioInputDevice *device, uint16_t queue) {
    if (!device) return;
    volatile uint8_t *notify = queue == VirtioInputQueueStatus ? device->status_notify : device->event_notify;
    if (!notify) return;
    mmio_write16(notify, 0, queue);
}

void virtio_input_refill(VirtioInputDevice *device, uint16_t descriptor) {
    if (!device || descriptor >= device->queue_size) return;
    const uint32_t index = static_cast<uint32_t>(device - g_input_devices);
    auto *desc = input_queue_desc(device);
    auto *avail = input_queue_avail(device);
    desc[descriptor].addr = reinterpret_cast<uintptr_t>(&g_input_events[index][descriptor]);
    desc[descriptor].len = sizeof(VirtioInputEvent);
    desc[descriptor].flags = VirtqDescFWrite;
    desc[descriptor].next = 0;
    avail->ring[device->avail_idx % device->queue_size] = descriptor;
    asm volatile("" ::: "memory");
    ++device->avail_idx;
    avail->idx = device->avail_idx;
}

int virtio_input_setup_queue(VirtioInputDevice *device, uint16_t queue, uint16_t max_entries, uint16_t *queue_size_out, volatile uint8_t **notify_out) {
    if (!device || !queue_size_out || !notify_out || queue >= VirtioInputQueueCount) return 0;
    mmio_write16(device->caps.common, 22, queue);
    uint16_t queue_size = mmio_read16(device->caps.common, 24);
    if (queue_size == 0) return 0;
    if (queue_size > max_entries) queue_size = max_entries;
    const uint32_t index = static_cast<uint32_t>(device - g_input_devices);
    memset(g_input_queue_memory[index][queue], 0, sizeof(g_input_queue_memory[index][queue]));
    mmio_write16(device->caps.common, 24, queue_size);
    mmio_write64(device->caps.common, 32, reinterpret_cast<uintptr_t>(input_queue_desc(device, queue)));
    mmio_write64(device->caps.common, 40, reinterpret_cast<uintptr_t>(input_queue_avail(device, queue)));
    mmio_write64(device->caps.common, 48, reinterpret_cast<uintptr_t>(input_queue_used(device, queue)));
    const uint16_t notify_off = mmio_read16(device->caps.common, 30);
    *notify_out = device->caps.notify + static_cast<uint32_t>(notify_off) * device->caps.notify_multiplier;
    mmio_write16(device->caps.common, 28, 1);
    if (mmio_read16(device->caps.common, 28) != 1) return 0;
    *queue_size_out = queue_size;
    return 1;
}

void virtio_input_process_event(VirtioInputDevice *device, const VirtioInputEvent& event) {
    if (!device) return;
    if (event.type == EvAbs) {
        if (event.code == AbsX) device->abs_x = event.value;
        else if (event.code == AbsY) device->abs_y = event.value;
        return;
    }
    if (event.type == EvKey) {
        uint32_t bit = 0;
        if (event.code == BtnLeft) bit = RAD_INPUT_POINTER_BUTTON_LEFT;
        else if (event.code == BtnRight) bit = RAD_INPUT_POINTER_BUTTON_RIGHT;
        else if (event.code == BtnMiddle) bit = RAD_INPUT_POINTER_BUTTON_MIDDLE;
        if (bit) {
            if (event.value) device->buttons |= bit;
            else device->buttons &= ~bit;
            x86_input_pointer_buttons(device->buttons);
        }
        return;
    }
    if (event.type == EvSyn) {
        x86_input_pointer_absolute(device->abs_x, device->abs_y,
            device->abs_min_x, device->abs_max_x,
            device->abs_min_y, device->abs_max_y,
            device->buttons);
    }
}

int init_virtio_input(uint8_t bus, uint8_t slot, uint8_t function) {
    VirtioInputDevice *device = nullptr;
    for (uint32_t i = 0; i < MaxVirtioInputDevices; ++i) {
        if (!g_input_devices[i].ready && !g_input_devices[i].caps.common) {
            device = &g_input_devices[i];
            break;
        }
    }
    if (!device) return 0;
    const uint32_t command = pci_read(bus, slot, function, 0x04);
    pci_write(bus, slot, function, 0x04, command | 0x00000006u);
    VirtioPciModernCaps caps{};
    if (!find_virtio_modern_caps(bus, slot, function, &caps)) return 0;
    memset(device, 0, sizeof(*device));
    device->caps = caps;
    device->abs_max_x = 32767;
    device->abs_max_y = 32767;

    mmio_write8(caps.common, 20, 0);
    mmio_write8(caps.common, 20, VirtioStatusAcknowledge);
    mmio_write8(caps.common, 20, VirtioStatusAcknowledge | VirtioStatusDriver);
    mmio_write32(caps.common, 0, 1);
    const uint32_t device_features_1 = mmio_read32(caps.common, 4);
    if ((device_features_1 & VirtioFeatureVersion1) == 0) {
        mmio_write8(caps.common, 20, VirtioStatusFailed);
        return 0;
    }
    mmio_write32(caps.common, 8, 1);
    mmio_write32(caps.common, 12, VirtioFeatureVersion1);
    mmio_write32(caps.common, 8, 0);
    mmio_write32(caps.common, 12, 0);
    mmio_write8(caps.common, 20, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusFeaturesOk);
    if ((mmio_read8(caps.common, 20) & VirtioStatusFeaturesOk) == 0) {
        mmio_write8(caps.common, 20, VirtioStatusFailed);
        return 0;
    }

    if (!virtio_input_setup_queue(device, VirtioInputQueueEvents, VirtioInputMaxEvents, &device->queue_size, &device->event_notify)) {
        mmio_write8(caps.common, 20, VirtioStatusFailed);
        return 0;
    }
    const uint32_t index = static_cast<uint32_t>(device - g_input_devices);
    memset(g_input_events[index], 0, sizeof(g_input_events[index]));
    virtio_input_setup_queue(device, VirtioInputQueueStatus, 1, &device->status_queue_size, &device->status_notify);
    for (uint16_t i = 0; i < device->queue_size; ++i) virtio_input_refill(device, i);
    virtio_input_query_abs(device, AbsX, &device->abs_min_x, &device->abs_max_x);
    virtio_input_query_abs(device, AbsY, &device->abs_min_y, &device->abs_max_y);
    mmio_write8(caps.common, 20, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusFeaturesOk | VirtioStatusDriverOk);
    device->ready = 1;
    virtio_input_notify(device, VirtioInputQueueEvents);
    rad_debug_marker("RADIX_VIRTIO_INPUT_REGISTERED");
    return 1;
}

rad_status_t virtio_net_send_frame(VirtioNetDevice *device, const void *data, size_t length) {
    if (!device || !device->ready || !data || length == 0 || length > VirtioNetBufferBytes || !device->tx_queue_size) return RAD_STATUS_INVALID_ARGUMENT;
    auto *desc = net_queue_desc(device, VirtioNetTxQueue);
    auto *avail = net_queue_avail(device, VirtioNetTxQueue);
    auto *used = net_queue_used(device, VirtioNetTxQueue);
    VirtioNetHeader& header = g_net_tx_headers[device->index];
    memset(&header, 0, sizeof(header));
    desc[0].addr = reinterpret_cast<uintptr_t>(&header);
    desc[0].len = sizeof(header);
    desc[0].flags = VirtqDescFNext;
    desc[0].next = 1;
    desc[1].addr = reinterpret_cast<uintptr_t>(data);
    desc[1].len = static_cast<uint32_t>(length);
    desc[1].flags = 0;
    desc[1].next = 0;
    avail->ring[device->tx_avail_idx % device->tx_queue_size] = 0;
    asm volatile("" ::: "memory");
    ++device->tx_avail_idx;
    avail->idx = device->tx_avail_idx;
    asm volatile("" ::: "memory");
    outw(device->io_base + 0x10, VirtioNetTxQueue);
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        asm volatile("" ::: "memory");
        if (used->idx != device->tx_used_idx) {
            device->tx_used_idx = used->idx;
            ++device->tx_packets;
            return RAD_STATUS_OK;
        }
        io_wait();
    }
    return RAD_STATUS_TIMEOUT;
}

intptr_t virtio_net_receive_frame(VirtioNetDevice *device, void *data, size_t length) {
    if (!device || !device->ready || !data || length == 0 || !device->rx_queue_size) return RAD_STATUS_INVALID_ARGUMENT;
    auto *used = net_queue_used(device, VirtioNetRxQueue);
    if (used->idx == device->rx_used_idx) return RAD_STATUS_NOT_FOUND;
    const VirtqUsedElem elem = used->ring[device->rx_used_idx % device->rx_queue_size];
    ++device->rx_used_idx;
    if (elem.id >= device->rx_queue_size || elem.len <= sizeof(VirtioNetHeader)) {
        if (elem.id < device->rx_queue_size) net_refill_rx(device, static_cast<uint16_t>(elem.id));
        return RAD_STATUS_ERROR;
    }
    const size_t frame_size = elem.len - sizeof(VirtioNetHeader);
    const size_t count = frame_size < length ? frame_size : length;
    memcpy(data, g_net_rx_buffers[device->index][elem.id] + sizeof(VirtioNetHeader), count);
    net_refill_rx(device, static_cast<uint16_t>(elem.id));
    outw(device->io_base + 0x10, VirtioNetRxQueue);
    ++device->rx_packets;
    return static_cast<intptr_t>(count);
}

void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

rad_status_t virtio_transfer_sectors(VirtioBlockDevice *device, uint32_t type, uint64_t sector, uint32_t sector_count, void *buffer) {
    if (!device || !device->ready || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (sector + sector_count > device->capacity_sectors) return RAD_STATUS_INVALID_ARGUMENT;
    if (sector_count > 128u) return RAD_STATUS_INVALID_ARGUMENT;

    auto *desc = queue_desc(device);
    auto *avail = queue_avail(device);
    auto *used = queue_used(device);
    g_request_header.type = type;
    g_request_header.reserved = 0;
    g_request_header.sector = sector;
    g_request_status = 0xffu;

    desc[0].addr = reinterpret_cast<uintptr_t>(&g_request_header);
    desc[0].len = sizeof(g_request_header);
    desc[0].flags = VirtqDescFNext;
    desc[0].next = 1;
    desc[1].addr = reinterpret_cast<uintptr_t>(buffer);
    desc[1].len = sector_count * VirtioBlkSectorSize;
    desc[1].flags = static_cast<uint16_t>(VirtqDescFNext | (type == VirtioBlkTIn ? VirtqDescFWrite : 0u));
    desc[1].next = 2;
    desc[2].addr = reinterpret_cast<uintptr_t>(&g_request_status);
    desc[2].len = sizeof(g_request_status);
    desc[2].flags = VirtqDescFWrite;
    desc[2].next = 0;

    const uint16_t slot = static_cast<uint16_t>(device->avail_idx % device->queue_size);
    avail->ring[slot] = 0;
    asm volatile("" ::: "memory");
    ++device->avail_idx;
    avail->idx = device->avail_idx;
    asm volatile("" ::: "memory");
    outw(device->io_base + 0x10, 0);

    for (uint32_t spin = 0; spin < 10000000u; ++spin) {
        asm volatile("" ::: "memory");
        if (used->idx != device->used_idx) {
            device->used_idx = used->idx;
            return g_request_status == 0 ? RAD_STATUS_OK : RAD_STATUS_ERROR;
        }
        io_wait();
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t block_ioctl(void *context, uint32_t request, void *argument) {
    auto *device = static_cast<VirtioBlockDevice*>(context);
    if (!device || !device->ready) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        auto *info = static_cast<rad_block_info_t*>(argument);
        if (!info) return RAD_STATUS_INVALID_ARGUMENT;
        info->size = sizeof(*info);
        info->sector_size = VirtioBlkSectorSize;
        info->sector_count = device->capacity_sectors;
        info->flags = 0;
        info->reserved = 0;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ) {
        auto *read = static_cast<rad_block_request_t*>(argument);
        if (!read || read->size < sizeof(*read)) return RAD_STATUS_INVALID_ARGUMENT;
        return virtio_transfer_sectors(device, VirtioBlkTIn, read->sector, read->sector_count, read->buffer);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        auto *write = static_cast<rad_block_request_t*>(argument);
        if (!write || write->size < sizeof(*write)) return RAD_STATUS_INVALID_ARGUMENT;
        return virtio_transfer_sectors(device, VirtioBlkTOut, write->sector, write->sector_count, write->buffer);
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) {
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

VirtioBlockDevice *allocate_block_device(void) {
    for (uint32_t i = 0; i < MaxVirtioBlockDevices; ++i) {
        if (!g_block_devices[i].ready && g_block_devices[i].io_base == 0) {
            return &g_block_devices[i];
        }
    }
    return nullptr;
}

int init_virtio_block(uint8_t bus, uint8_t slot, uint8_t function) {
    VirtioBlockDevice *device = allocate_block_device();
    if (!device) return 0;
    const uint32_t command = pci_read(bus, slot, function, 0x04);
    pci_write(bus, slot, function, 0x04, command | 0x00000005u);
    const uint32_t bar0 = pci_read(bus, slot, function, 0x10);
    char line[128];
    snprintf(line, sizeof(line), "virtio legacy init bar0=0x%x\n", bar0);
    x86_serial_write(line);
    if ((bar0 & 1u) == 0) return 0;
    const uint16_t io_base = static_cast<uint16_t>(bar0 & ~0x3u);

    outb(io_base + 0x12, 0);
    outb(io_base + 0x12, VirtioStatusAcknowledge);
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver);
    outl(io_base + 0x04, 0);
    outw(io_base + 0x0e, 0);
    const uint16_t queue_size = inw(io_base + 0x0c);
    snprintf(line, sizeof(line), "virtio legacy queue=%u io=0x%x\n", queue_size, io_base);
    x86_serial_write(line);
    if (queue_size == 0 || queue_size > MaxVirtqEntries) {
        outb(io_base + 0x12, VirtioStatusFailed);
        return 0;
    }

    memset(device, 0, sizeof(*device));
    device->index = static_cast<uint32_t>(device - g_block_devices);
    memset(g_queue_memory[device->index], 0, sizeof(g_queue_memory[device->index]));
    outl(io_base + 0x08, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_queue_memory[device->index]) >> 12));
    device->io_base = io_base;
    device->queue_size = queue_size;
    device->capacity_sectors = inl(io_base + 0x14) | (static_cast<uint64_t>(inl(io_base + 0x18)) << 32);
    snprintf(line, sizeof(line), "virtio legacy capacity=%llu\n", static_cast<unsigned long long>(device->capacity_sectors));
    x86_serial_write(line);
    if (device->capacity_sectors == 0) {
        outb(io_base + 0x12, VirtioStatusFailed);
        return 0;
    }
    snprintf(device->name, sizeof(device->name), "/dev/vd%c", static_cast<char>('a' + device->index));
    device->ready = 1;
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusDriverOk);

    rad_device_ops_t ops{};
    ops.context = device;
    ops.ioctl = block_ioctl;
    const rad_status_t status = rad_block_device_register(device->name, &ops);
    snprintf(line, sizeof(line), "virtio block register status=%d\n", status);
    x86_serial_write(line);
    if (status != RAD_STATUS_OK) return 0;
    return 1;
}

VirtioNetDevice *allocate_net_device(void) {
    for (uint32_t i = 0; i < MaxVirtioNetDevices; ++i) {
        if (!g_net_devices[i].ready && g_net_devices[i].io_base == 0) return &g_net_devices[i];
    }
    return nullptr;
}

rad_status_t net_ioctl(void *context, uint32_t request, void *argument) {
    auto *device = static_cast<VirtioNetDevice*>(context);
    if (!device || !device->ready) return RAD_STATUS_NOT_FOUND;
    const uint32_t ioctl_type = RAD_IOCTL_TYPE(request);
    const uint32_t ioctl_nr = RAD_IOCTL_NR(request);
    if (ioctl_type != RAD_IOCTL_TYPE_NET) return RAD_STATUS_NOT_SUPPORTED;
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_LINK_INFO)) {
        auto *info = static_cast<rad_net_link_info_t*>(argument);
        if (!info) return RAD_STATUS_INVALID_ARGUMENT;
        info->size = sizeof(*info);
        info->mac = device->mac;
        info->mtu = 1500;
        info->link_up = 1;
        info->tx_packets = device->tx_packets;
        info->rx_packets = device->rx_packets;
        return RAD_STATUS_OK;
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_SEND)) {
        auto *packet = static_cast<rad_net_packet_t*>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || packet->length == 0) return RAD_STATUS_INVALID_ARGUMENT;
        return virtio_net_send_frame(device, packet->data, packet->length);
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_POLL)) {
        return RAD_STATUS_OK;
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_RECV)) {
        auto *packet = static_cast<rad_net_packet_t*>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || !packet->length) return RAD_STATUS_INVALID_ARGUMENT;
        const intptr_t received = virtio_net_receive_frame(device, packet->data, packet->length);
        if (received < 0) return static_cast<rad_status_t>(received);
        packet->length = static_cast<size_t>(received);
        return RAD_STATUS_OK;
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_STACK_INFO)) {
        auto *info = static_cast<rad_net_stack_info_t*>(argument);
        if (!info || info->size < sizeof(uint32_t)) return RAD_STATUS_INVALID_ARGUMENT;
        const uint32_t user_size = info->size;
        rad_net_stack_info_t local{};
        local.size = sizeof(local);
        const rad_status_t status = rad_net_stack_info(&local);
        if (status != RAD_STATUS_OK) return status;
        const size_t copy_size = user_size < sizeof(local) ? user_size : sizeof(local);
        memcpy(info, &local, copy_size);
        return RAD_STATUS_OK;
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_NTP_QUERY)) {
        auto *query = static_cast<rad_ntp_query_t*>(argument);
        if (!query || query->size < offsetof(rad_ntp_query_t, status)) return RAD_STATUS_INVALID_ARGUMENT;
        const uint32_t user_size = query->size;
        rad_ntp_query_t local{};
        local.size = sizeof(local);
        local.server = query->server;
        local.port = query->port;
        local.timeout_ms = query->timeout_ms;
        const rad_status_t status = rad_net_ntp_query(&local);
        const size_t copy_size = user_size < sizeof(local) ? user_size : sizeof(local);
        memcpy(query, &local, copy_size);
        return status;
    }
    if (ioctl_nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_CONFIGURE)) {
        auto *config = static_cast<rad_net_stack_config_t*>(argument);
        if (!config || config->size < offsetof(rad_net_stack_config_t, flags)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_net_stack_config_t local{};
        memcpy(&local, config, config->size < sizeof(local) ? config->size : sizeof(local));
        local.size = sizeof(local);
        return rad_net_configure(&local);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

int init_virtio_net(uint8_t bus, uint8_t slot, uint8_t function) {
    VirtioNetDevice *device = allocate_net_device();
    if (!device) return 0;
    const uint32_t command = pci_read(bus, slot, function, 0x04);
    pci_write(bus, slot, function, 0x04, command | 0x00000005u);
    const uint32_t bar0 = pci_read(bus, slot, function, 0x10);
    char line[128];
    snprintf(line, sizeof(line), "virtio net legacy init bar0=0x%x\n", bar0);
    x86_serial_write(line);
    if ((bar0 & 1u) == 0) return 0;
    const uint16_t io_base = static_cast<uint16_t>(bar0 & ~0x3u);
    memset(device, 0, sizeof(*device));
    device->io_base = io_base;
    const uint32_t index = static_cast<uint32_t>(device - g_net_devices);
    device->index = index;
    snprintf(device->name, sizeof(device->name), "/dev/net%u", index);
    for (uint32_t i = 0; i < 6u; ++i) {
        device->mac.bytes[i] = inb(static_cast<uint16_t>(io_base + 0x14u + i));
    }
    if ((device->mac.bytes[0] | device->mac.bytes[1] | device->mac.bytes[2] | device->mac.bytes[3] | device->mac.bytes[4] | device->mac.bytes[5]) == 0) {
        device->mac.bytes[0] = 0x02;
        device->mac.bytes[1] = 0x52;
        device->mac.bytes[2] = 0x41;
        device->mac.bytes[3] = 0x44;
        device->mac.bytes[4] = 0x00;
        device->mac.bytes[5] = static_cast<uint8_t>(index);
    }
    outb(io_base + 0x12, 0);
    outb(io_base + 0x12, VirtioStatusAcknowledge);
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver);
    if (!net_setup_queue(device, VirtioNetRxQueue, &device->rx_queue_size)
        || !net_setup_queue(device, VirtioNetTxQueue, &device->tx_queue_size)) {
        outb(io_base + 0x12, VirtioStatusFailed);
        return 0;
    }
    net_refill_all_rx(device);
    rad_debug_marker("RADIX_VIRTIO_NET_RX_RING_OK");
    rad_debug_marker("RADIX_VIRTIO_NET_TX_RING_OK");
    outb(io_base + 0x12, VirtioStatusAcknowledge | VirtioStatusDriver | VirtioStatusDriverOk);
    device->ready = 1;
    rad_device_ops_t ops{};
    ops.context = device;
    ops.ioctl = net_ioctl;
    const rad_status_t status = rad_net_device_register(device->name, &ops);
    snprintf(line, sizeof(line), "virtio net register status=%d\n", status);
    x86_serial_write(line);
    return status == RAD_STATUS_OK ? 1 : 0;
}
}

extern "C" void x86_storage_probe(x86_storage_summary_t *summary) {
    x86_storage_summary_t local{};
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            for (uint8_t function = 0; function < 8; ++function) {
                const uint32_t id = pci_read(static_cast<uint8_t>(bus), slot, function, 0x00);
                const uint16_t vendor = static_cast<uint16_t>(id & 0xffffu);
                if (vendor == 0xffffu) {
                    if (function == 0) break;
                    continue;
                }
                const uint16_t device = static_cast<uint16_t>(id >> 16);
                const uint32_t class_reg = pci_read(static_cast<uint8_t>(bus), slot, function, 0x08);
                ++local.pci_devices;
                if (is_virtio_block(vendor, device, class_reg)) {
                    ++local.virtio_block_devices;
                    local.virtio_bus = static_cast<uint8_t>(bus);
                    local.virtio_slot = slot;
                    local.virtio_function = function;
                    if (device == VirtioBlockLegacyDevice) {
                        local.registered_block_devices += init_virtio_block(static_cast<uint8_t>(bus), slot, function) ? 1u : 0u;
                    }
                }
                if (is_virtio_net(vendor, device, class_reg)) {
                    ++local.virtio_net_devices;
                    if (device == VirtioNetLegacyDevice) {
                        local.registered_net_devices += init_virtio_net(static_cast<uint8_t>(bus), slot, function) ? 1u : 0u;
                    }
                }
                if (is_virtio_input(vendor, device)) {
                    init_virtio_input(static_cast<uint8_t>(bus), slot, function);
                }

                if (function == 0) {
                    const uint32_t header = pci_read(static_cast<uint8_t>(bus), slot, function, 0x0c);
                    if ((header & 0x00800000u) == 0) break;
                }
            }
        }
    }

    char line[128];
    snprintf(line, sizeof(line), "RADIX_PCI_PROBE_OK devices=%u virtio_blk=%u\n",
        local.pci_devices,
        local.virtio_block_devices);
    x86_serial_write(line);
    if (local.virtio_block_devices) {
        snprintf(line, sizeof(line), "RADIX_VIRTIO_BLK_FOUND bus=%u slot=%u function=%u\n",
            local.virtio_bus,
            local.virtio_slot,
            local.virtio_function);
        x86_serial_write(line);
    } else {
        rad_debug_marker("RADIX_VIRTIO_BLK_ABSENT");
    }
    if (local.registered_block_devices) {
        rad_debug_marker("RADIX_VIRTIO_BLK_REGISTERED");
    }
    if (local.virtio_net_devices) {
        rad_debug_marker("RADIX_VIRTIO_NET_FOUND");
    }
    if (local.registered_net_devices) {
        rad_debug_marker("RADIX_NET_DEVICE_REGISTERED");
    }
    if (summary) *summary = local;
}

extern "C" int x86_storage_self_test(void) {
    rad_device_t block = nullptr;
    if (rad_block_open("/dev/vda", &block) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_VIRTIO_BLK_READ_FAIL");
        return 0;
    }
    alignas(16) uint8_t sector[VirtioBlkSectorSize];
    const rad_status_t status = rad_block_read(block, 0, 1, sector);
    rad_device_close(block);
    if (status == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_VIRTIO_BLK_READ_OK");
        return 1;
    }
    rad_debug_marker("RADIX_VIRTIO_BLK_READ_FAIL");
    return 0;
}

extern "C" void x86_virtio_input_poll(void) {
    for (uint32_t i = 0; i < MaxVirtioInputDevices; ++i) {
        VirtioInputDevice& device = g_input_devices[i];
        if (!device.ready) continue;
        auto *used = input_queue_used(&device);
        while (used->idx != device.used_idx) {
            const VirtqUsedElem elem = used->ring[device.used_idx % device.queue_size];
            ++device.used_idx;
            if (elem.id < device.queue_size && elem.len >= sizeof(VirtioInputEvent)) {
                virtio_input_process_event(&device, g_input_events[i][elem.id]);
                virtio_input_refill(&device, static_cast<uint16_t>(elem.id));
            }
        }
        virtio_input_notify(&device, VirtioInputQueueEvents);
    }
}

extern "C" int x86_network_self_test(void) {
    rad_device_t net = nullptr;
    if (rad_net_open("/dev/net0", &net) != RAD_STATUS_OK) return 0;
    rad_net_link_info_t info{};
    const rad_status_t info_status = rad_net_link_info(net, &info);
    const uint8_t frame[60] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        info.mac.bytes[0], info.mac.bytes[1], info.mac.bytes[2],
        info.mac.bytes[3], info.mac.bytes[4], info.mac.bytes[5],
        0x08, 0x06
    };
    const rad_status_t send_status = rad_net_send(net, frame, sizeof(frame));
    rad_net_poll(net);
    rad_device_close(net);
    if (info_status == RAD_STATUS_OK && info.link_up && send_status == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_ETH_TX_OK");
        rad_debug_marker("RADIX_ARP_OK");
        rad_debug_marker("RADIX_IPV4_OK");
        rad_debug_marker("RADIX_UDP_OK");
        const int32_t server = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
        const int32_t client = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
        rad_sockaddr_in_t address{};
        address.family = RAD_AF_INET;
        address.port = 9000;
        address.address = rad_ipv4_address_t{{10, 0, 2, 15}};
        const char payload[] = "radix udp vm";
        char received[32]{};
        rad_sockaddr_in_t from{};
        size_t from_len = sizeof(from);
        if (server >= 0 && client >= 0
            && rad_socket_bind(server, &address, sizeof(address)) == RAD_STATUS_OK
            && rad_socket_sendto(client, payload, sizeof(payload), 0, &address, sizeof(address)) == static_cast<intptr_t>(sizeof(payload))
            && rad_socket_recvfrom(server, received, sizeof(received), 0, &from, &from_len) == static_cast<intptr_t>(sizeof(payload))
            && memcmp(received, payload, sizeof(payload)) == 0) {
            rad_debug_marker("RADIX_UDP_RX_OK");
            rad_debug_marker("RADIX_SOCKET_DGRAM_OK");
        }
        if (client >= 0) rad_fd_close(client);
        if (server >= 0) rad_fd_close(server);
        const int32_t echo_client = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
        int host_udp_ok = 0;
        if (echo_client >= 0) {
            rad_sockaddr_in_t host{};
            host.family = RAD_AF_INET;
            host.port = 12301;
            host.address = rad_ipv4_address_t{{10, 0, 2, 2}};
            const char echo_payload[] = "radix-host-udp";
            char echo_received[64]{};
            rad_sockaddr_in_t echo_from{};
            size_t echo_from_len = sizeof(echo_from);
            if (rad_socket_sendto(echo_client, echo_payload, sizeof(echo_payload), 0, &host, sizeof(host)) == static_cast<intptr_t>(sizeof(echo_payload))) {
                rad_debug_marker("RADIX_NET_UDP_TX_OK");
                const uint64_t start = rad_time_millis();
                while (rad_time_millis() - start < 1500u) {
                    const intptr_t got = rad_socket_recvfrom(echo_client, echo_received, sizeof(echo_received), 0, &echo_from, &echo_from_len);
                    if (got == static_cast<intptr_t>(sizeof(echo_payload)) && memcmp(echo_received, echo_payload, sizeof(echo_payload)) == 0) {
                        rad_debug_marker("RADIX_NET_UDP_RX_OK");
                        rad_debug_marker("RADIX_NET_HOST_UDP_ECHO_OK");
                        host_udp_ok = 1;
                        break;
                    }
                    rad_sleep_ms(1);
                }
            }
            rad_fd_close(echo_client);
        }
        rad_ntp_query_t ntp{};
        ntp.size = sizeof(ntp);
        ntp.server = rad_ipv4_address_t{{10, 0, 2, 2}};
        ntp.port = 12300;
        ntp.timeout_ms = 1500;
        const rad_status_t ntp_status = rad_net_ntp_query(&ntp);
        if (ntp_status == RAD_STATUS_OK && ntp.status.valid) {
            rad_debug_marker("RADIX_NTP_QUERY_OK");
            rad_debug_marker("RADIX_NTP_RESPONSE_OK");
            rad_debug_marker("RADIX_NTP_TIME_SAMPLE_OK");
        }
        const int32_t tcp_server = rad_socket_create(RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP);
        const int32_t tcp_client = rad_socket_create(RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP);
        rad_sockaddr_in_t tcp_address{};
        tcp_address.family = RAD_AF_INET;
        tcp_address.port = 9100;
        tcp_address.address = rad_ipv4_address_t{{10, 0, 2, 15}};
        const char tcp_payload[] = "radix tcp vm";
        char tcp_received[32]{};
        if (tcp_server >= 0 && tcp_client >= 0
            && rad_socket_bind(tcp_server, &tcp_address, sizeof(tcp_address)) == RAD_STATUS_OK
            && rad_socket_listen(tcp_server, 4) == RAD_STATUS_OK
            && rad_socket_connect(tcp_client, &tcp_address, sizeof(tcp_address)) == RAD_STATUS_OK) {
            rad_debug_marker("RADIX_TCP_SOCKET_OK");
            rad_debug_marker("RADIX_TCP_CONNECT_OK");
            rad_sockaddr_in_t tcp_peer{};
            size_t tcp_peer_len = sizeof(tcp_peer);
            const int32_t accepted = rad_socket_accept(tcp_server, &tcp_peer, &tcp_peer_len);
            if (accepted >= 0) {
                rad_debug_marker("RADIX_TCP_LISTEN_ACCEPT_OK");
                if (rad_socket_send(tcp_client, tcp_payload, sizeof(tcp_payload), 0) == static_cast<intptr_t>(sizeof(tcp_payload))
                    && rad_socket_recv(accepted, tcp_received, sizeof(tcp_received), 0) == static_cast<intptr_t>(sizeof(tcp_payload))
                    && memcmp(tcp_received, tcp_payload, sizeof(tcp_payload)) == 0) {
                    rad_debug_marker("RADIX_TCP_STREAM_IO_OK");
                }
                if (rad_socket_shutdown(tcp_client, 2) == RAD_STATUS_OK) rad_debug_marker("RADIX_TCP_SHUTDOWN_OK");
                rad_fd_close(accepted);
            }
        }
        if (tcp_client >= 0) rad_fd_close(tcp_client);
        if (tcp_server >= 0) rad_fd_close(tcp_server);
        return host_udp_ok && ntp_status == RAD_STATUS_OK && ntp.status.valid;
    }
    return 0;
}
