#!/usr/bin/env python3
"""RADPx-OS Pi Zero 2 W hardware serial console + marker gate.

The board's PL011 console (GPIO14/15, routed by config.txt's disable-bt) reaches
this host as a USB CDC serial port -- either a plain UART->USB adapter or the UART
bridge on the Pico DirtyJTAG. This mirrors the QEMU smoke on real hardware:

  ./serial_console.py                 # interactive passthrough (like screen)
  ./serial_console.py --gate          # capture ~25 s, check expected-markers.txt
  ./serial_console.py --login         # type root/rad, confirm RAD_LOGIN_OK
  ./serial_console.py -p /dev/ttyACM0 --baud 115200

Autodetects /dev/ttyACM* then /dev/ttyUSB* when -p is omitted.
"""
import argparse, glob, os, sys, time
try:
    import serial
except ImportError:
    sys.exit("pyserial missing: pip install pyserial")

HERE = os.path.dirname(os.path.abspath(__file__))
GATE = os.path.join(HERE, "..", "rad_pi_zero2w", "expected-markers.txt")


def autodetect():
    for pat in ("/dev/ttyACM*", "/dev/ttyUSB*"):
        hits = sorted(glob.glob(pat))
        if hits:
            return hits[0]
    sys.exit("no /dev/ttyACM* or /dev/ttyUSB* found; pass -p")


def open_port(port, baud):
    port = port or autodetect()
    print(f"[serial_console] {port} @ {baud}", file=sys.stderr)
    return serial.Serial(port, baud, timeout=0.2)


def read_gate(ser, seconds):
    want = [l.strip() for l in open(GATE) if l.strip() and not l.startswith("#")]
    seen, buf, deadline = set(), b"", time.time() + seconds
    while time.time() < deadline:
        buf += ser.read(4096)
        for line in buf.split(b"\n"):
            s = line.decode("latin1", "replace").strip()
            if s in want:
                seen.add(s)
        sys.stdout.buffer.write(ser.read(0))  # keep flush cheap
    missing = [m for m in want if m not in seen]
    print(f"[gate] {len(seen)}/{len(want)} markers")
    if missing:
        print("[gate] MISSING: " + " ".join(missing[:12]) + (" ..." if len(missing) > 12 else ""))
        return 1
    print("[gate] PASS")
    return 0


def drive_login(ser, user, pw, timeout):
    def wait(pat, secs):
        end, buf = time.time() + secs, b""
        while time.time() < end:
            buf += ser.read(4096)
            if pat.encode() in buf:
                return True
        return False
    if not wait("RAD_LOGIN_SPAWN_OK", 40):
        print("[login] login session never spawned"); return 1
    time.sleep(12)                       # let boot-session settle + login prompt
    ser.write((user + "\r").encode()); time.sleep(2)
    ser.write((pw + "\r").encode())
    if wait("RAD_LOGIN_OK", timeout):
        print("[login] RAD_LOGIN_OK -- PASS"); return 0
    print("[login] RAD_LOGIN_OK not seen -- FAIL"); return 1


def interactive(ser):
    import termios, tty, select
    print("[serial_console] interactive (Ctrl-] to quit)", file=sys.stderr)
    old = termios.tcgetattr(sys.stdin)
    try:
        tty.setraw(sys.stdin)
        while True:
            r, _, _ = select.select([sys.stdin, ser.fileno()], [], [], 0.1)
            if sys.stdin in r:
                c = os.read(sys.stdin.fileno(), 1)
                if c == b"\x1d":
                    break
                ser.write(c)
            if ser.fileno() in r:
                os.write(sys.stdout.fileno(), ser.read(4096))
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--gate", action="store_true")
    ap.add_argument("--login", action="store_true")
    ap.add_argument("--seconds", type=int, default=25)
    ap.add_argument("--user", default="root")
    ap.add_argument("--password", default="rad")
    a = ap.parse_args()
    ser = open_port(a.port, a.baud)
    if a.gate:
        sys.exit(read_gate(ser, a.seconds))
    if a.login:
        sys.exit(drive_login(ser, a.user, a.password, a.seconds))
    interactive(ser)


if __name__ == "__main__":
    main()
