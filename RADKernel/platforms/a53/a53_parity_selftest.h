// Shared a53 parity self-tests. These exercise portable kernel subsystems that
// carry x86<->a53 parity markers and are independent of any SoC (ZynqMP GEM /
// bcm283x) or of a mounted rootfs, so every a53 board entry can gate the same
// markers regardless of its hardware. Networking here is strictly in-guest
// loopback (delivered in-memory by the runtime socket layer), so it needs no
// NIC -- the pi Zero 2 W has no GEM-equivalent, yet still gates the L4 set.
#ifndef RAD_A53_PARITY_SELFTEST_H
#define RAD_A53_PARITY_SELFTEST_H

#ifdef __cplusplus
extern "C" {
#endif

// Kernel infrastructure parity: module lifecycle, deferred work, wait queues.
// Emits RAD_MODULE_LIFECYCLE_OK, RAD_DEFERRED_WORK_OK, RAD_WAIT_QUEUE_OK.
void rad_a53_kernel_infra_selftest(void);

// In-guest networking L4 parity (NIC-independent loopback): UDP datagram and
// TCP stream round-trips through the portable socket->IPv4 stack. Emits
// RAD_IPV4_OK, RAD_UDP_OK, RAD_UDP_RX_OK, RAD_SOCKET_DGRAM_OK and
// RAD_TCP_SOCKET/CONNECT/LISTEN_ACCEPT/STREAM_IO/SHUTDOWN_OK. Returns non-zero
// when both the UDP and TCP round-trips complete (so a board can gate its
// network-service milestone on the loopback stack being fully operational).
int rad_a53_net_loopback_l4_selftest(void);

#ifdef __cplusplus
}
#endif

#endif // RAD_A53_PARITY_SELFTEST_H
