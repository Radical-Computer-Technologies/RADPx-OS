#!/usr/bin/env python3
import argparse
import select
import socket
import struct
import time


NTP_EPOCH_DELTA = 2_208_988_800


def build_ntp_response(request: bytes) -> bytes:
    response = bytearray(48)
    response[0] = 0x24
    response[1] = 1
    response[2] = 6
    response[3] = 0xEC
    now = time.time()
    seconds = int(now) + NTP_EPOCH_DELTA
    fraction = int((now - int(now)) * (1 << 32)) & 0xFFFFFFFF
    if len(request) >= 48:
        response[24:32] = request[40:48]
    struct.pack_into("!II", response, 32, seconds, fraction)
    struct.pack_into("!II", response, 40, seconds, fraction)
    return bytes(response)


def bind_udp(port: int) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    sock.setblocking(False)
    return sock


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ntp-port", type=int, default=12300)
    parser.add_argument("--echo-port", type=int, default=12301)
    parser.add_argument("--duration", type=float, default=120.0)
    args = parser.parse_args()

    ntp = bind_udp(args.ntp_port)
    echo = bind_udp(args.echo_port)
    deadline = time.monotonic() + args.duration
    print(f"RADIX_HOST_NET_RESPONDER_READY ntp={args.ntp_port} echo={args.echo_port}", flush=True)
    try:
        while time.monotonic() < deadline:
            readable, _, _ = select.select([ntp, echo], [], [], 0.2)
            for sock in readable:
                data, address = sock.recvfrom(2048)
                if sock is echo:
                    sock.sendto(data, address)
                else:
                    sock.sendto(build_ntp_response(data), address)
    finally:
        ntp.close()
        echo.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
