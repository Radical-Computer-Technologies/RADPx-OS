# Experimental Networking {#networking}

RADix-OS Crimson 0.2.0 includes an experimental networking path for the x86 VM
target. The goal of this surface is to prove the kernel interfaces for network
devices, packet receive, POSIX-inspired datagram sockets, host-verified UDP,
SNTP/NTP sampling, and the first TCP stream socket lifecycle before adding a
complete wire-level TCP/IP stack or target-specific production drivers.

## Current Stack

The network device API exposes link information, frame transmit, frame receive,
and polling. The x86 VM target registers a legacy virtio-net device as
`/dev/net0` when QEMU exposes one. The VM backend now programs separate legacy
virtio RX and TX rings and emits smoke markers when the rings are owned by the
guest.

The minimal IPv4 path now includes Ethernet II dispatch, ARP request/reply and
cache learning, IPv4 checksum validation, UDP transmit/receive, ICMP echo
reply, static default-gateway routing, IPv4 DNS A-record lookup, and
SNTP/NTP sampling. RADix uses this path for host UDP echo verification and can
query either the deterministic host-side test responder or public NTP servers.

The socket layer currently supports:

- `RAD_AF_INET`
- `RAD_SOCK_DGRAM`
- `RAD_IPPROTO_UDP`
- `socket`, `bind`, `sendto`, `recvfrom`
- `getaddrinfo`, `freeaddrinfo`, `gethostbyname`
- `inet_aton`, `inet_pton(AF_INET)`, `inet_ntop(AF_INET)`
- `RAD_SOCK_STREAM`
- `RAD_IPPROTO_TCP`
- `listen`, `accept`, `connect`, `send`, `recv`, `shutdown`
- fd lifecycle through `close`, `dup`, `dup2`, `fcntl`, fork inheritance, and
  close-on-exec handling

The x86 sysroot also exposes minimal `sys/socket.h`, `netinet/in.h`, and
`arpa/inet.h` headers plus libc wrappers for the socket calls above.

The TCP support in this pass is a kernel socket model and local stream
foundation. It verifies listener setup, client connect, accept, connected peer
state, stream byte delivery, and shutdown behavior. It is not yet a full
wire-level TCP implementation with SYN/ACK sequencing, retransmit timers,
congestion control, route lookup, or external host connections.

The default VM IPv4 profile is static and QEMU-oriented:

- guest: `10.0.2.15`
- netmask: `255.255.255.0`
- gateway/host side: `10.0.2.2`
- default DNS resolver: `10.0.2.3`
- default SNTP/NTP target: `time.google.com:123`
- MTU: `1500`

RADBuild/rkconfig can override these with `network_enabled`, `net_ipv4`,
`net_netmask`, `net_gateway`, `net_ntp_server`, `net_ntp_port`, and
`net_dns_server`. Deterministic VM smoke runs may still override NTP to
`10.0.2.2:12300` and use the host UDP/NTP responder.

The `rash` shell includes:

- `net` to show `/dev/net0`, static IPv4 state, counters, ARP cache count, and
  default NTP target
- `ntpdate [-q] [host-or-ip] [port]` to run a userspace UDP SNTP/NTP query,
  resolve hostnames through DNS, and set the realtime clock unless `-q` is used

## Current Limits

This page is intentionally marked experimental. UDP datagrams, local TCP stream
sockets, virtio-net queue setup, host UDP echo, and NTP sampling are verified
through x86 VM smoke markers, but the network stack is still minimal. DHCP,
IPv6, DNS caching, TCP DNS fallback, full wire-level TCP, richer route tables,
firewall rules, packet sockets, and non-x86 NIC drivers remain future work.

## Verification

Host tests cover network device registration, link info, transmit, receive, UDP
datagram socket delivery, and the local TCP listener/connect/accept/stream
socket lifecycle.

The x86 Slint VM smoke gate checks:

- `RADIX_VIRTIO_NET_FOUND`
- `RADIX_NET_DEVICE_REGISTERED`
- `RADIX_VIRTIO_NET_TX_RING_OK`
- `RADIX_VIRTIO_NET_RX_RING_OK`
- `RADIX_ETH_TX_OK`
- `RADIX_ARP_OK`
- `RADIX_IPV4_OK`
- `RADIX_UDP_OK`
- `RADIX_UDP_RX_OK`
- `RADIX_NET_UDP_TX_OK`
- `RADIX_NET_UDP_RX_OK`
- `RADIX_NET_HOST_UDP_ECHO_OK`
- `RADIX_NTP_QUERY_OK`
- `RADIX_NTP_RESPONSE_OK`
- `RADIX_NTP_TIME_SAMPLE_OK`
- `RADIX_SOCKET_DGRAM_OK`
- `RADIX_TCP_SOCKET_OK`
- `RADIX_TCP_CONNECT_OK`
- `RADIX_TCP_LISTEN_ACCEPT_OK`
- `RADIX_TCP_STREAM_IO_OK`
- `RADIX_TCP_SHUTDOWN_OK`
