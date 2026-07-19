// Cadence GEM (Gigabit Ethernet MAC) driver for the Xilinx ZynqMP / ZuBoard-1CG
// A53 platform. Structurally mirrors the x86_64 virtio-net driver
// (platforms/x86_64/x86_storage.cpp): a polled NIC that plugs into the portable
// net stack through the generic rad_device_ops_t.ioctl contract (LINK_INFO /
// SEND / POLL / RECV; STACK_INFO / NTP / CONFIGURE delegate to the core stack).
// It registers as "/dev/net0", the device the runtime net stack pulls frames
// from (RADKernel_runtime.cpp poll_network_for_udp).
//
// The GEM MMIO block at 0xff0b0000 (GEM0) is already identity-mapped as device
// memory by the a53 kernel tables (kernel_identity_device_region covers
// [0xe0000000,0x100000000)). Buffer descriptor rings and packet buffers live in
// normal kernel .bss; since the kernel runs at EL1 with a flat identity map
// (PA == VA) and is linked in the low 32-bit range, their virtual addresses are
// also the physical addresses the GEM DMA engine programs into its 32-bit
// descriptor pointers.
//
// Bring-up validation uses the MAC's local-loopback mode (NWCTRL.LOOPBACK_LOCAL):
// a self-test frame transmitted with loopback enabled is fed straight back into
// the receive path, exercising both DMA directions deterministically without any
// dependency on the host network backend. Loopback is cleared afterwards so the
// device operates normally for real traffic.

#include <radkernel/radkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void rad_hal_sleep_us(uint32_t microseconds);

namespace {

// ---------------------------------------------------------------------------
// Register map (byte offsets from the GEM base). ZynqMP GEM0.
// ---------------------------------------------------------------------------
constexpr uintptr_t GemBase = 0xff0b0000u;

constexpr uintptr_t RegNwCtrl = 0x000u;   // Network control
constexpr uintptr_t RegNwCfg = 0x004u;    // Network configuration
constexpr uintptr_t RegDmaCfg = 0x010u;   // DMA configuration
constexpr uintptr_t RegRxQBase = 0x018u;  // RX buffer-descriptor queue base
constexpr uintptr_t RegTxQBase = 0x01cu;  // TX buffer-descriptor queue base
constexpr uintptr_t RegRxStatus = 0x020u; // RX status (write-1-to-clear)
constexpr uintptr_t RegTxStatus = 0x014u; // TX status (write-1-to-clear)
constexpr uintptr_t RegIntStatus = 0x024u; // Interrupt status (write-1-to-clear)
constexpr uintptr_t RegIntEnable = 0x028u; // Interrupt enable  (write-1-to-set)
constexpr uintptr_t RegIntDisable = 0x02cu; // Interrupt disable (write-1-to-mask)
constexpr uintptr_t RegSpAddr1Lo = 0x088u;  // Specific address 1, low  (MAC[0..3])
constexpr uintptr_t RegSpAddr1Hi = 0x08cu;  // Specific address 1, high (MAC[4..5])

// GEM interrupt bits (ISR/IER/IDR share the layout). Bit 1 = frame received.
constexpr uint32_t GemIntRxComplete = 1u << 1u;
// RX status: bit 1 = frame received (write-1-to-clear).
constexpr uint32_t RxStatusFrameReceived = 1u << 1u;
// GEM0 GIC interrupt ID. QEMU's xlnx-zynqmp uses gem_intr[0]=57 as an SPI index
// (gic_spi[57]); SPIs start at INTID 32, so the actual GIC INTID is 57 + 32 = 89.
constexpr uint32_t GemIntid = 89u;

extern "C" rad_status_t rad_zynqmp_gic_configure_spi(uint32_t intid, uint8_t priority, uint8_t cpu_target);

// NWCTRL bits.
constexpr uint32_t NwCtrlLoopbackLocal = 1u << 1u;
constexpr uint32_t NwCtrlEnableRx = 1u << 2u;
constexpr uint32_t NwCtrlEnableTx = 1u << 3u;
constexpr uint32_t NwCtrlMgmtEnable = 1u << 4u;
constexpr uint32_t NwCtrlClearStats = 1u << 5u;
constexpr uint32_t NwCtrlTxStart = 1u << 9u;

// NWCFG bits.
constexpr uint32_t NwCfgSpeed100 = 1u << 0u;
constexpr uint32_t NwCfgFullDuplex = 1u << 1u;
constexpr uint32_t NwCfgFcsRemove = 1u << 17u;
constexpr uint32_t NwCfgMdcDiv48 = 3u << 18u; // MDC = pclk/48 (irrelevant in QEMU)

// DMACFG: RX buffer size is expressed in 64-byte units at bits [23:16];
// bits [4:0] set the AHB burst length.
constexpr uint32_t DmaCfgBurst16 = 0x10u;

// Descriptor word bits.
constexpr uint32_t Desc0RxOwnership = 1u << 0u; // 1 => software owns (frame ready)
constexpr uint32_t Desc0RxWrap = 1u << 1u;      // last descriptor in the ring
constexpr uint32_t Desc0AddrMask = ~0x3u;       // low 2 bits are flags, not address
constexpr uint32_t Desc1Used = 1u << 31u;       // TX: 1 => hardware processed it
constexpr uint32_t Desc1TxWrap = 1u << 30u;     // TX: last descriptor in the ring
constexpr uint32_t Desc1TxLast = 1u << 15u;     // TX: last buffer of this frame
constexpr uint32_t Desc1LengthMask = 0x1fffu;   // 13-bit frame length (RX and TX)

// ---------------------------------------------------------------------------
// Ring / buffer geometry.
// ---------------------------------------------------------------------------
constexpr uint32_t RxRingCount = 8u;
constexpr uint32_t TxRingCount = 4u;
constexpr uint32_t RxBufferBytes = 1536u;               // 24 * 64-byte units
constexpr uint32_t RxBufSizeUnits = RxBufferBytes / 64u; // DMACFG[23:16]
constexpr uint32_t FrameMax = 1536u;
constexpr uint32_t TxSpinLimit = 1000000u;

struct GemDescriptor {
    uint32_t word0;
    uint32_t word1;
};

struct GemDevice {
    uintptr_t base;
    char name[16];
    rad_mac_address_t mac;
    uint32_t rx_index; // next RX descriptor to inspect
    uint32_t tx_index; // next TX descriptor to hand to the DMA engine
    uint64_t tx_packets;
    uint64_t rx_packets;
    int ready;
};

// DMA-visible structures. 64-byte aligned so descriptor queue-base low bits stay
// clear and each RX buffer starts on a cache line.
alignas(64) GemDescriptor g_rx_ring[RxRingCount];
alignas(64) GemDescriptor g_tx_ring[TxRingCount];
alignas(64) uint8_t g_rx_buffers[RxRingCount][RxBufferBytes];
alignas(64) uint8_t g_tx_buffer[FrameMax];
GemDevice g_gem;

// RX interrupt state. The handler runs in IRQ context and only masks + flags;
// the consumer (gem_receive_frame) drains descriptors and re-arms. So no
// descriptor state is touched from IRQ context -> no races with the poller.
volatile uint32_t g_gem_rx_irq_enabled = 0u; // driver armed the RX-complete IRQ
volatile uint32_t g_gem_rx_irq_seen = 0u;    // at least one RX IRQ has fired
volatile uint64_t g_gem_rx_irq_count = 0u;

// ---------------------------------------------------------------------------
// MMIO + DMA cache maintenance helpers.
// ---------------------------------------------------------------------------
inline volatile uint32_t *gem_reg(GemDevice *device, uintptr_t offset) {
    return reinterpret_cast<volatile uint32_t *>(device->base + offset);
}

inline uint32_t gem_read(GemDevice *device, uintptr_t offset) {
    return *gem_reg(device, offset);
}

inline void gem_write(GemDevice *device, uintptr_t offset, uint32_t value) {
    *gem_reg(device, offset) = value;
    asm volatile("dsb sy" ::: "memory");
}

// Clean+invalidate a range to the point of coherency so the GEM DMA master sees
// CPU writes and the CPU sees DMA writes. A no-op under QEMU (no cache model),
// load-bearing on silicon; matches the dc civac pattern used elsewhere in the
// zynqmp HAL for the SMP release flags.
inline void dma_sync(const void *pointer, size_t bytes) {
    uintptr_t address = reinterpret_cast<uintptr_t>(pointer) & ~static_cast<uintptr_t>(63u);
    const uintptr_t end = reinterpret_cast<uintptr_t>(pointer) + bytes;
    for (; address < end; address += 64u) {
        asm volatile("dc civac, %0" ::"r"(address) : "memory");
    }
    asm volatile("dsb sy" ::: "memory");
}

// ---------------------------------------------------------------------------
// Descriptor ring setup.
// ---------------------------------------------------------------------------
void gem_reset_rings(GemDevice *device) {
    for (uint32_t i = 0; i < RxRingCount; ++i) {
        uint32_t word0 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_rx_buffers[i])) & Desc0AddrMask;
        if (i == RxRingCount - 1u) word0 |= Desc0RxWrap;
        g_rx_ring[i].word0 = word0; // ownership bit clear => hardware owns it
        g_rx_ring[i].word1 = 0u;
    }
    for (uint32_t i = 0; i < TxRingCount; ++i) {
        g_tx_ring[i].word0 = 0u;
        // USED set => software owns it; the DMA engine skips it until a send
        // clears USED. Wrap marks the ring end.
        g_tx_ring[i].word1 = Desc1Used | (i == TxRingCount - 1u ? Desc1TxWrap : 0u);
    }
    device->rx_index = 0u;
    device->tx_index = 0u;
    dma_sync(g_rx_ring, sizeof(g_rx_ring));
    dma_sync(g_tx_ring, sizeof(g_tx_ring));
}

// ---------------------------------------------------------------------------
// TX / RX frame paths.
// ---------------------------------------------------------------------------
rad_status_t gem_send_frame(GemDevice *device, const void *data, size_t length) {
    if (!device || !device->ready || !data || length == 0u || length > FrameMax) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    const uint32_t slot = device->tx_index % TxRingCount;
    memcpy(g_tx_buffer, data, length);
    dma_sync(g_tx_buffer, length);

    GemDescriptor *descriptor = &g_tx_ring[slot];
    descriptor->word0 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_tx_buffer));
    uint32_t word1 = (static_cast<uint32_t>(length) & Desc1LengthMask) | Desc1TxLast;
    if (slot == TxRingCount - 1u) word1 |= Desc1TxWrap;
    descriptor->word1 = word1; // USED clear => hardware owns it now
    dma_sync(descriptor, sizeof(*descriptor));

    gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) | NwCtrlTxStart);

    for (uint32_t spin = 0; spin < TxSpinLimit; ++spin) {
        dma_sync(descriptor, sizeof(*descriptor));
        if (descriptor->word1 & Desc1Used) {
            ++device->tx_index;
            ++device->tx_packets;
            return RAD_STATUS_OK;
        }
        asm volatile("" ::: "memory");
    }
    return RAD_STATUS_TIMEOUT;
}

intptr_t gem_receive_frame(GemDevice *device, void *data, size_t length) {
    if (!device || !device->ready || !data || length == 0u) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t slot = device->rx_index % RxRingCount;
    GemDescriptor *descriptor = &g_rx_ring[slot];
    dma_sync(descriptor, sizeof(*descriptor));
    if ((descriptor->word0 & Desc0RxOwnership) == 0u) return RAD_STATUS_NOT_FOUND;

    const size_t frame_length = descriptor->word1 & Desc1LengthMask;
    const uintptr_t buffer = descriptor->word0 & Desc0AddrMask;
    const size_t count = frame_length < length ? frame_length : length;
    if (count > 0u) {
        dma_sync(reinterpret_cast<void *>(buffer), count);
        memcpy(data, reinterpret_cast<void *>(buffer), count);
    }

    // Hand the descriptor back to the hardware: clear ownership, preserve the
    // buffer address and wrap flag.
    descriptor->word0 &= ~Desc0RxOwnership;
    descriptor->word1 = 0u;
    dma_sync(descriptor, sizeof(*descriptor));

    ++device->rx_index;
    ++device->rx_packets;
    // A descriptor was freed. If interrupt-driven RX is armed, the handler masked
    // RX-complete when it fired; re-enable it so the next inbound frame
    // re-interrupts (level-sensitive: if one is already pending it fires again
    // immediately, handler re-masks, no storm).
    if (g_gem_rx_irq_enabled) gem_write(device, RegIntEnable, GemIntRxComplete);
    return static_cast<intptr_t>(count);
}

// ---------------------------------------------------------------------------
// Controller bring-up.
// ---------------------------------------------------------------------------
void gem_hw_init(GemDevice *device) {
    // Quiesce: disable RX/TX, mask every interrupt (this driver is polled), clear
    // sticky status, reset the statistics counters.
    gem_write(device, RegNwCtrl, 0u);
    gem_write(device, RegIntDisable, 0xffffffffu);
    gem_write(device, RegRxStatus, 0x0000000fu);
    gem_write(device, RegTxStatus, 0x000001ffu);
    gem_write(device, RegNwCtrl, NwCtrlClearStats);

    gem_write(device, RegNwCfg, NwCfgSpeed100 | NwCfgFullDuplex | NwCfgFcsRemove | NwCfgMdcDiv48);
    gem_write(device, RegDmaCfg, (RxBufSizeUnits << 16u) | DmaCfgBurst16);

    // Program the station MAC into specific-address-1 (RX unicast filter).
    const uint8_t *mac = device->mac.bytes;
    gem_write(device, RegSpAddr1Lo,
              static_cast<uint32_t>(mac[0]) | (static_cast<uint32_t>(mac[1]) << 8u) |
                  (static_cast<uint32_t>(mac[2]) << 16u) | (static_cast<uint32_t>(mac[3]) << 24u));
    gem_write(device, RegSpAddr1Hi,
              static_cast<uint32_t>(mac[4]) | (static_cast<uint32_t>(mac[5]) << 8u));

    gem_reset_rings(device);
    gem_write(device, RegRxQBase, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_rx_ring)));
    gem_write(device, RegTxQBase, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_tx_ring)));

    gem_write(device, RegNwCtrl, NwCtrlMgmtEnable | NwCtrlEnableRx | NwCtrlEnableTx);
}

// Deterministic DMA self-test: loop a frame TX -> RX through the MAC's local
// loopback and confirm it returns intact. Independent of any host backend.
rad_status_t gem_loopback_selftest(GemDevice *device) {
    gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) | NwCtrlLoopbackLocal);

    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    memset(&frame[0], 0xff, 6);              // destination: broadcast
    memcpy(&frame[6], device->mac.bytes, 6); // source: our MAC
    frame[12] = 0x88;                         // ethertype 0x88b5 (local experimental)
    frame[13] = 0xb5;
    static const char kTag[] = "RAD-GEM-LOOPBACK";
    memcpy(&frame[14], kTag, sizeof(kTag));

    rad_status_t tx = gem_send_frame(device, frame, sizeof(frame));
    if (tx != RAD_STATUS_OK) {
        gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) & ~NwCtrlLoopbackLocal);
        return tx;
    }
    rad_debug_marker("RAD_GEM_TX_OK");

    rad_status_t result = RAD_STATUS_TIMEOUT;
    uint8_t received[FrameMax];
    for (uint32_t attempt = 0; attempt < 100000u; ++attempt) {
        const intptr_t got = gem_receive_frame(device, received, sizeof(received));
        if (got == RAD_STATUS_NOT_FOUND) {
            asm volatile("" ::: "memory");
            continue;
        }
        if (got < 0) { result = static_cast<rad_status_t>(got); break; }
        rad_debug_marker("RAD_GEM_RX_OK");
        if (static_cast<size_t>(got) >= 14u + sizeof(kTag) &&
            memcmp(&received[14], kTag, sizeof(kTag)) == 0) {
            result = RAD_STATUS_OK;
        } else {
            result = RAD_STATUS_ERROR;
        }
        break;
    }

    // Return to normal (non-loopback) operation regardless of the outcome.
    gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) & ~NwCtrlLoopbackLocal);
    device->tx_packets = 0u;
    device->rx_packets = 0u;
    return result;
}

// Best-effort real-traffic probe: broadcast an ARP request for the SLIRP
// gateway (10.0.2.2) and wait for the reply. Unlike the loopback self-test
// (which stays inside the MAC), this drives a frame OUT to the QEMU network
// backend and receives one back, validating the full external path. Never fails
// boot -- bare metal has no SLIRP responder, so a timeout is expected there.
void gem_slirp_arp_probe(GemDevice *device) {
    uint8_t frame[42];
    memset(frame, 0, sizeof(frame));
    // Ethernet header.
    memset(&frame[0], 0xff, 6);               // destination: broadcast
    memcpy(&frame[6], device->mac.bytes, 6);  // source: our MAC
    frame[12] = 0x08; frame[13] = 0x06;       // ethertype: ARP
    // ARP payload (IPv4 over Ethernet, request).
    frame[14] = 0x00; frame[15] = 0x01;       // htype: Ethernet
    frame[16] = 0x08; frame[17] = 0x00;       // ptype: IPv4
    frame[18] = 0x06; frame[19] = 0x04;       // hlen, plen
    frame[20] = 0x00; frame[21] = 0x01;       // opcode: request
    memcpy(&frame[22], device->mac.bytes, 6); // sender MAC
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15; // sender IP 10.0.2.15
    // target MAC left zero (frame[32..37])
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;  // target IP 10.0.2.2

    if (gem_send_frame(device, frame, sizeof(frame)) != RAD_STATUS_OK) return;

    uint8_t reply[FrameMax];
    for (uint32_t attempt = 0; attempt < 60000u; ++attempt) {
        const intptr_t got = gem_receive_frame(device, reply, sizeof(reply));
        if (got == RAD_STATUS_NOT_FOUND) {
            rad_hal_sleep_us(10); // let the emulated backend run and reply
            continue;
        }
        if (got < 0) return;
        // ARP reply for 10.0.2.2?
        if (static_cast<size_t>(got) >= 42u &&
            reply[12] == 0x08 && reply[13] == 0x06 &&    // ethertype ARP
            reply[20] == 0x00 && reply[21] == 0x02 &&    // opcode reply
            reply[28] == 10 && reply[29] == 0 && reply[30] == 2 && reply[31] == 2) {
            rad_debug_marker("RAD_GEM_ARP_GATEWAY_OK");
            return;
        }
        // Some other frame arrived; keep looking.
    }
}

// Real-traffic-via-stack self-test. Unlike gem_slirp_arp_probe (which hand-builds
// raw L2 frames and drives the MAC directly), this exercises the portable
// socket -> UDP -> IPv4 -> ARP stack (RADKernel_runtime.cpp). A datagram sent to
// the SLIRP gateway 10.0.2.2 is NON-local, so rad_socket_sendto routes it through
// send_udp_frame, which resolves the gateway's MAC via a real ARP exchange over
// THIS device and then transmits the UDP frame. So the stack's traffic egresses
// through GEM (TX) and the ARP reply ingresses through GEM (RX); both are proven
// by the device packet counters moving. Deterministic under QEMU user-net (SLIRP
// always answers the gateway ARP -- same guarantee that gates
// RAD_GEM_ARP_GATEWAY_OK). A no-op if sockets/net aren't ready (fd < 0).
void gem_stack_traffic_selftest(GemDevice *device) {
    const uint64_t tx0 = device->tx_packets;
    const uint64_t rx0 = device->rx_packets;
    const int32_t fd = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    if (fd < 0) return;
    rad_sockaddr_in_t gateway{};
    gateway.family = RAD_AF_INET;
    gateway.port = 9u; // discard
    gateway.address = rad_ipv4_address_t{{10u, 0u, 2u, 2u}};
    static const char payload[] = "rad-gem-stack";
    (void)rad_socket_sendto(fd, payload, sizeof(payload), 0u, &gateway, sizeof(gateway));
    // Drain inbound so the ARP reply (received via GEM by the stack's poll) is
    // accounted; also lets any datagram land.
    char scratch[64];
    rad_sockaddr_in_t from{};
    size_t from_len = sizeof(from);
    for (int i = 0; i < 8; ++i) {
        (void)rad_socket_recvfrom(fd, scratch, sizeof(scratch), 0u, &from, &from_len);
    }
    rad_fd_close(fd);
    if (device->tx_packets > tx0) rad_debug_marker("RAD_GEM_STACK_TX_OK");
    if (device->rx_packets > rx0) rad_debug_marker("RAD_GEM_STACK_RX_OK");
}

// In-guest socket-datagram parity self-test. Mirrors the x86 path
// (x86_storage.cpp): bind a UDP server on the guest IP and loop a datagram
// through the portable socket -> UDP -> IPv4 stack (RADKernel_runtime.cpp), which
// recognises its own address and delivers locally without egressing. Proves the
// L4 socket API works on a53 identically to x86 -- emits the same
// RAD_IPV4_OK / RAD_UDP_OK / RAD_UDP_RX_OK / RAD_SOCKET_DGRAM_OK markers. A no-op
// if sockets/net are not ready (fd < 0).
void gem_socket_dgram_selftest() {
    const int32_t server = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    const int32_t client = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    if (server < 0 || client < 0) {
        if (server >= 0) rad_fd_close(server);
        if (client >= 0) rad_fd_close(client);
        return;
    }
    rad_sockaddr_in_t address{};
    address.family = RAD_AF_INET;
    address.port = 9000u;
    address.address = rad_ipv4_address_t{{10u, 0u, 2u, 15u}};
    static const char payload[] = "rad-a53-udp";
    char received[32]{};
    rad_sockaddr_in_t from{};
    size_t from_len = sizeof(from);
    const bool bound = rad_socket_bind(server, &address, sizeof(address)) == RAD_STATUS_OK;
    const bool sent = rad_socket_sendto(client, payload, sizeof(payload), 0u, &address, sizeof(address))
        == static_cast<intptr_t>(sizeof(payload));
    if (bound && sent) {
        rad_debug_marker("RAD_IPV4_OK");
        rad_debug_marker("RAD_UDP_OK");
        if (rad_socket_recvfrom(server, received, sizeof(received), 0u, &from, &from_len)
                == static_cast<intptr_t>(sizeof(payload))
            && memcmp(received, payload, sizeof(payload)) == 0) {
            rad_debug_marker("RAD_UDP_RX_OK");
            rad_debug_marker("RAD_SOCKET_DGRAM_OK");
        }
    }
    rad_fd_close(client);
    rad_fd_close(server);
}

// RX-complete interrupt handler (GIC SPI 57). Runs in IRQ context, so it stays
// minimal and never touches the descriptor ring (the poller owns that -- keeping
// them apart avoids any IRQ/thread race). It acknowledges the GEM interrupt,
// masks RX-complete so nothing re-fires before the frame is drained, and records
// liveness. gem_receive_frame re-arms RX-complete once it frees a descriptor.
void gem_rx_irq_handler(uint32_t, void *arg) {
    auto *device = static_cast<GemDevice *>(arg);
    const uint32_t status = gem_read(device, RegIntStatus);
    gem_write(device, RegIntStatus, status); // write-1-clear the asserted bits
    if (status & GemIntRxComplete) {
        gem_write(device, RegIntDisable, GemIntRxComplete);      // mask until drained
        gem_write(device, RegRxStatus, RxStatusFrameReceived);   // clear RX status
        g_gem_rx_irq_seen = 1u;
        __atomic_add_fetch(&g_gem_rx_irq_count, 1u, __ATOMIC_RELAXED);
    }
}

// ---------------------------------------------------------------------------
// rad_device_ops_t.ioctl contract (mirrors x86 net_ioctl).
// ---------------------------------------------------------------------------
rad_status_t gem_net_ioctl(void *context, uint32_t request, void *argument) {
    auto *device = static_cast<GemDevice *>(context);
    if (!device || !device->ready) return RAD_STATUS_NOT_FOUND;
    if (RAD_IOCTL_TYPE(request) != RAD_IOCTL_TYPE_NET) return RAD_STATUS_NOT_SUPPORTED;
    const uint32_t nr = RAD_IOCTL_NR(request);

    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_LINK_INFO)) {
        auto *info = static_cast<rad_net_link_info_t *>(argument);
        if (!info) return RAD_STATUS_INVALID_ARGUMENT;
        info->size = sizeof(*info);
        info->mac = device->mac;
        info->mtu = 1500u;
        info->link_up = 1;
        info->tx_packets = device->tx_packets;
        info->rx_packets = device->rx_packets;
        return RAD_STATUS_OK;
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_SEND)) {
        auto *packet = static_cast<rad_net_packet_t *>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || packet->length == 0u) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        return gem_send_frame(device, packet->data, packet->length);
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_POLL)) {
        return RAD_STATUS_OK;
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_RECV)) {
        auto *packet = static_cast<rad_net_packet_t *>(argument);
        if (!packet || packet->size < sizeof(*packet) || !packet->data || !packet->length) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        const intptr_t received = gem_receive_frame(device, packet->data, packet->length);
        if (received < 0) return static_cast<rad_status_t>(received);
        packet->length = static_cast<size_t>(received);
        return RAD_STATUS_OK;
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_STACK_INFO)) {
        auto *info = static_cast<rad_net_stack_info_t *>(argument);
        if (!info || info->size < sizeof(uint32_t)) return RAD_STATUS_INVALID_ARGUMENT;
        const uint32_t user_size = info->size;
        rad_net_stack_info_t local{};
        local.size = sizeof(local);
        const rad_status_t status = rad_net_stack_info(&local);
        if (status != RAD_STATUS_OK) return status;
        memcpy(info, &local, user_size < sizeof(local) ? user_size : sizeof(local));
        return RAD_STATUS_OK;
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_NTP_QUERY)) {
        auto *query = static_cast<rad_ntp_query_t *>(argument);
        if (!query || query->size < offsetof(rad_ntp_query_t, status)) return RAD_STATUS_INVALID_ARGUMENT;
        const uint32_t user_size = query->size;
        rad_ntp_query_t local{};
        local.size = sizeof(local);
        local.server = query->server;
        local.port = query->port;
        local.timeout_ms = query->timeout_ms;
        const rad_status_t status = rad_net_ntp_query(&local);
        memcpy(query, &local, user_size < sizeof(local) ? user_size : sizeof(local));
        return status;
    }
    if (nr == RAD_IOCTL_NR(RAD_DEVICE_IOCTL_NET_CONFIGURE)) {
        auto *config = static_cast<rad_net_stack_config_t *>(argument);
        if (!config || config->size < offsetof(rad_net_stack_config_t, flags)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_net_stack_config_t local{};
        memcpy(&local, config, config->size < sizeof(local) ? config->size : sizeof(local));
        local.size = sizeof(local);
        return rad_net_configure(&local);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry point: probe, self-test, and register "/dev/net0".
// Called from rad_hal_register_default_devices during kernel bring-up.
// ---------------------------------------------------------------------------
// Bring up interrupt-driven RX and validate it end to end. Called from the board
// boot AFTER the GIC and CPU interrupts are live (rad_zynqmp_preempt_init). Routes
// GEM0's SPI to CPU0, registers the handler, arms RX-complete at the GEM, then
// local-loopbacks a frame so a receive raises a real interrupt through the GIC.
// Leaves the RX interrupt armed (the poller re-arms after each drain), so RX is
// interrupt-driven from here on with the polled path as fallback.
extern "C" rad_status_t rad_zynqmp_gem_rx_irq_selftest(void) {
    GemDevice *device = &g_gem;
    if (!device->ready) return RAD_STATUS_NOT_INITIALIZED;
    rad_zynqmp_gic_configure_spi(GemIntid, 0xa0u, 0x01u); // prio below timer, CPU0
    const rad_status_t reg = rad_irq_register(GemIntid, "gem0-rx", gem_rx_irq_handler, device);
    if (reg != RAD_STATUS_OK) return reg;
    (void)rad_irq_enable(GemIntid);
    g_gem_rx_irq_seen = 0u;
    g_gem_rx_irq_enabled = 1u;
    gem_write(device, RegIntStatus, 0xffffffffu);       // clear any stale status
    gem_write(device, RegIntEnable, GemIntRxComplete);  // arm RX-complete

    // Deterministic trigger: local-loopback a tagged frame back into our own MAC.
    gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) | NwCtrlLoopbackLocal);
    uint8_t frame[64];
    memset(frame, 0, sizeof(frame));
    memset(&frame[0], 0xff, 6);
    memcpy(&frame[6], device->mac.bytes, 6);
    frame[12] = 0x88; frame[13] = 0xb5;
    static const char kTag[] = "RAD-GEM-RXIRQ";
    memcpy(&frame[14], kTag, sizeof(kTag));
    (void)gem_send_frame(device, frame, sizeof(frame));

    rad_status_t result = RAD_STATUS_TIMEOUT;
    for (uint32_t i = 0; i < 200000u && !g_gem_rx_irq_seen; ++i) {
        asm volatile("" ::: "memory");
        rad_hal_sleep_us(1);
    }
    gem_write(device, RegNwCtrl, gem_read(device, RegNwCtrl) & ~NwCtrlLoopbackLocal);
    if (g_gem_rx_irq_seen) {
        rad_debug_marker("RAD_GEM_RX_IRQ_OK");
        result = RAD_STATUS_OK;
    }
    // Drain the loopback frame (also re-arms RX-complete for steady-state use).
    uint8_t scratch[FrameMax];
    while (gem_receive_frame(device, scratch, sizeof(scratch)) >= 0) {}
    device->rx_packets = 0u;
    device->tx_packets = 0u;
    return result;
}

extern "C" rad_status_t rad_zynqmp_gem_init(void) {
    GemDevice *device = &g_gem;
    memset(device, 0, sizeof(*device));
    device->base = GemBase;
    // Locally-administered MAC 02:52:41:44:00:01 ("RAD" + unit), matching the
    // x86 virtio-net fallback scheme.
    device->mac.bytes[0] = 0x02;
    device->mac.bytes[1] = 0x52;
    device->mac.bytes[2] = 0x41;
    device->mac.bytes[3] = 0x44;
    device->mac.bytes[4] = 0x00;
    device->mac.bytes[5] = 0x01;
    memcpy(device->name, "/dev/net0", sizeof("/dev/net0"));

    gem_hw_init(device);
    device->ready = 1;
    rad_debug_marker("RAD_GEM_INIT_OK");

    const rad_status_t loop = gem_loopback_selftest(device);
    if (loop == RAD_STATUS_OK) {
        rad_debug_marker("RAD_GEM_LOOPBACK_OK");
    } else {
        rad_debug_marker("RAD_GEM_LOOPBACK_FAIL");
    }

    // Best-effort real-traffic check over the (now non-loopback) link.
    gem_slirp_arp_probe(device);
    device->tx_packets = 0;
    device->rx_packets = 0;

    rad_device_ops_t ops{};
    ops.context = device;
    ops.ioctl = gem_net_ioctl;
    const rad_status_t status = rad_net_device_register(device->name, &ops);
    if (status == RAD_STATUS_OK) {
        rad_debug_marker("RAD_GEM_NET0_REGISTERED");
        // Now that /dev/net0 is registered, drive real traffic through the IP/UDP
        // stack (not just raw L2 loopback/ARP) and confirm it egresses/ingresses
        // GEM.
        gem_stack_traffic_selftest(device);
        // L4 socket-datagram parity: prove the portable socket/UDP/IPv4 API on a53
        // (in-guest loopback, mirrors the x86 selftest markers).
        gem_socket_dgram_selftest();
    } else {
        rad_debug_marker("RAD_GEM_NET0_REGISTER_FAIL");
    }
    return status;
}
