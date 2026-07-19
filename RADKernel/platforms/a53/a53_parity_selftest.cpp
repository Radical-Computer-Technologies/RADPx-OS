#include "a53_parity_selftest.h"

#include <radkernel/radkernel.h>

#include <stdint.h>
#include <string.h>

// Shared a53 parity self-tests -- see a53_parity_selftest.h. Uses only the
// portable rad_* kernel API so it links identically on every a53 backend
// (ZynqMP / bcm283x). The zuboard board entry carries an inline copy of the
// kernel-infra checks; this file is the reusable form the pi Zero 2 W links.

namespace {
unsigned g_module_probe_inits = 0;
unsigned g_module_probe_exits = 0;
rad_status_t module_probe_init(void *) { ++g_module_probe_inits; return RAD_STATUS_OK; }
void module_probe_exit(void *) { ++g_module_probe_exits; }
unsigned g_deferred_work_ran = 0;
void deferred_work_handler(void *) { ++g_deferred_work_ran; }
} // namespace

extern "C" void rad_a53_kernel_infra_selftest(void) {
    // Module lifecycle: register a probe module, see it listed as initialized,
    // then unregister -- gates RAD_MODULE_LIFECYCLE_OK.
    rad_module_descriptor_t d{};
    d.size = sizeof(d);
    d.name = "rad_a53_probe";
    d.bus = "a53";
    d.compatible = "rad,a53-probe";
    d.init = module_probe_init;
    d.exit = module_probe_exit;
    rad_module_info_t mods[4]{};
    const rad_status_t reg = rad_module_register(&d);
    const size_t n = rad_module_list(mods, 4);
    int listed = 0;
    for (size_t i = 0; i < n && i < 4; ++i) {
        if (strcmp(mods[i].name, "rad_a53_probe") == 0
            && mods[i].state == RAD_MODULE_INITIALIZED
            && mods[i].last_status == RAD_STATUS_OK) listed = 1;
    }
    const rad_status_t unreg = rad_module_unregister("rad_a53_probe");
    if (reg == RAD_STATUS_OK && unreg == RAD_STATUS_OK && listed
        && g_module_probe_inits == 1 && g_module_probe_exits == 1) {
        rad_debug_marker("RAD_MODULE_LIFECYCLE_OK");
    }

    // Deferred work: submit a handler, poll it to completion.
    size_t ran = 0;
    if (rad_work_submit("a53-boot-self-test", deferred_work_handler, nullptr) == RAD_STATUS_OK
        && rad_work_poll(8, &ran) == RAD_STATUS_OK && ran > 0 && g_deferred_work_ran == 1) {
        rad_debug_marker("RAD_DEFERRED_WORK_OK");
    }

    // Wait queue: create, wake, wait, destroy.
    rad_wait_queue_t q = nullptr;
    const rad_status_t cr = rad_wait_queue_create(&q);
    const rad_status_t wk = cr == RAD_STATUS_OK ? rad_wait_queue_wake_one(q) : RAD_STATUS_ERROR;
    const rad_status_t wt = cr == RAD_STATUS_OK ? rad_wait_queue_wait(q, 1) : RAD_STATUS_ERROR;
    if (q) rad_wait_queue_destroy(q);
    if (cr == RAD_STATUS_OK && wk == RAD_STATUS_OK && wt == RAD_STATUS_OK) {
        rad_debug_marker("RAD_WAIT_QUEUE_OK");
    }
}

extern "C" int rad_a53_net_loopback_l4_selftest(void) {
    int udp_ok = 0;
    int tcp_ok = 0;
    // UDP loopback: bind a server on the guest IP, send a datagram from a client
    // to it, and confirm it arrives intact. The runtime socket layer delivers a
    // datagram addressed to the local stack IP in-memory (no frame ever leaves
    // the guest), so this needs no NIC. Guest IP matches the rkconfig default
    // (10.0.2.15), the same address the ZynqMP GEM self-test uses.
    const int32_t server = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    const int32_t client = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    if (server >= 0 && client >= 0) {
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
                udp_ok = 1;
            }
        }
    }
    if (client >= 0) rad_fd_close(client);
    if (server >= 0) rad_fd_close(server);

    // TCP loopback: server on the guest IP, connect a client, accept, stream a
    // payload, and shut down -- all local through the portable socket->TCP->IPv4
    // stack, no host responder.
    const int32_t tcp_server = rad_socket_create(RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP);
    const int32_t tcp_client = rad_socket_create(RAD_AF_INET, RAD_SOCK_STREAM, RAD_IPPROTO_TCP);
    rad_sockaddr_in_t tcp_addr{};
    tcp_addr.family = RAD_AF_INET;
    tcp_addr.port = 9100u;
    tcp_addr.address = rad_ipv4_address_t{{10u, 0u, 2u, 15u}};
    static const char tcp_payload[] = "rad-a53-tcp";
    char tcp_received[32]{};
    if (tcp_server >= 0 && tcp_client >= 0
        && rad_socket_bind(tcp_server, &tcp_addr, sizeof(tcp_addr)) == RAD_STATUS_OK
        && rad_socket_listen(tcp_server, 4) == RAD_STATUS_OK
        && rad_socket_connect(tcp_client, &tcp_addr, sizeof(tcp_addr)) == RAD_STATUS_OK) {
        rad_debug_marker("RAD_TCP_SOCKET_OK");
        rad_debug_marker("RAD_TCP_CONNECT_OK");
        rad_sockaddr_in_t tcp_peer{};
        size_t tcp_peer_len = sizeof(tcp_peer);
        const int32_t accepted = rad_socket_accept(tcp_server, &tcp_peer, &tcp_peer_len);
        if (accepted >= 0) {
            rad_debug_marker("RAD_TCP_LISTEN_ACCEPT_OK");
            if (rad_socket_send(tcp_client, tcp_payload, sizeof(tcp_payload), 0) == static_cast<intptr_t>(sizeof(tcp_payload))
                && rad_socket_recv(accepted, tcp_received, sizeof(tcp_received), 0) == static_cast<intptr_t>(sizeof(tcp_payload))
                && memcmp(tcp_received, tcp_payload, sizeof(tcp_payload)) == 0) {
                rad_debug_marker("RAD_TCP_STREAM_IO_OK");
                tcp_ok = 1;
            }
            if (rad_socket_shutdown(tcp_client, 2) == RAD_STATUS_OK) rad_debug_marker("RAD_TCP_SHUTDOWN_OK");
            rad_fd_close(accepted);
        }
    }
    if (tcp_client >= 0) rad_fd_close(tcp_client);
    if (tcp_server >= 0) rad_fd_close(tcp_server);
    return udp_ok && tcp_ok;
}
