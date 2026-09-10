#!/usr/bin/env python3
"""Serial smoke tests for OTA confirm (issue #42 / #44)."""
import argparse
import os
import select
import sys
import termios
import time

NEEDLES_IDLE = (
    "Setup complete",
    "pending verification",
    "Hello heartbeat OK",
    "Room map from hello",
    "Waiting for approval",
    "Hello failed",
)

NEEDLES_HEARTBEAT = (
    "Hello heartbeat OK",
    "Room map from hello",
    "Hello OK",
)

NEEDLES_OTA_OK = (
    "[ota] === SUCCESS ===",
    "[main] OTA success",
)

NEEDLES_OTA_FAIL = (
    "[main] OTA failed",
    "[ota] FAILED",
)


def open_serial(path, attempts=40):
    last = None
    for _ in range(attempts):
        if not os.path.exists(path):
            time.sleep(0.25)
            continue
        try:
            fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            attrs = termios.tcgetattr(fd)
            attrs[0] = 0
            attrs[1] = 0
            attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attrs[3] = 0
            attrs[4] = termios.B115200
            attrs[5] = termios.B115200
            cc = list(attrs[6])
            cc[termios.VMIN] = 0
            cc[termios.VTIME] = 0
            attrs[6] = cc
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            try:
                termios.tcflush(fd, termios.TCIOFLUSH)
            except termios.error:
                pass
            return fd
        except OSError as e:
            last = e
            time.sleep(0.25)
    raise SystemExit(f"cannot open {path}: {last}")


def reopen_serial(path, timeout):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        try:
            return open_serial(path, attempts=4)
        except SystemExit as e:
            last = e
            time.sleep(0.4)
    raise SystemExit(f"serial {path} did not return after reboot: {last}")


def read_more(fd, timeout, buf):
    deadline = time.time() + timeout
    while time.time() < deadline:
        wait = min(0.2, max(0, deadline - time.time()))
        r, _, _ = select.select([fd], [], [], wait)
        if not r:
            continue
        try:
            chunk = os.read(fd, 4096)
        except OSError:
            chunk = b""
        if not chunk:
            continue
        text = chunk.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
        buf.append(text)


def wait_for(fd, needles, timeout, buf):
    deadline = time.time() + timeout
    while time.time() < deadline:
        read_more(fd, min(0.3, max(0, deadline - time.time())), buf)
        text = "".join(buf)
        for n in needles:
            if n in text:
                return n, text
    return None, "".join(buf)


def send(fd, cmd):
    os.write(fd, (cmd + "\r\n").encode())
    sys.stdout.write(f"\n>>> {cmd}\n")
    sys.stdout.flush()


def wait_idle(fd, buf, timeout=45):
    print("--- wait idle ---", flush=True)
    hit, text = wait_for(fd, NEEDLES_IDLE, timeout, buf)
    if hit is None:
        print("\nTIMEOUT waiting for boot/idle log", flush=True)
        sys.exit(2)
    pending = "pending verification" in text or "OTA boot" in text
    print(f"\n[hil] boot hit={hit!r} pending_verify={pending}", flush=True)
    return pending, text


def run_dry_run(fd, buf, skip_nohello):
    time.sleep(1)
    read_more(fd, 0.5, buf)
    buf.clear()
    send(fd, "confirm_test")
    started, _ = wait_for(fd, ["OTA confirm dry-run", "Unknown command", "confirm_test only from idle"], 20, buf)
    if started != "OTA confirm dry-run":
        print(f"\n[hil] confirm_test start hit={started!r}", flush=True)
        sys.exit(4)
    hit, _ = wait_for(
        fd,
        [
            "Boot confirmed via hello (dry-run)",
            "Boot confirmed via button (dry-run)",
        ],
        25,
        buf,
    )
    print(f"\n[hil] confirm_test hit={hit!r}", flush=True)
    if hit != "Boot confirmed via hello (dry-run)":
        sys.exit(4)

    if skip_nohello:
        print("[hil] PASS: hello dry-run", flush=True)
        return

    # Wait until IDLE is reading serial again (a blocking hello can eat the next command).
    time.sleep(2)
    read_more(fd, 1.0, buf)
    buf.clear()
    send(fd, "confirm_test nohello")
    started, _ = wait_for(fd, ["hello skipped", "Unknown command"], 25, buf)
    if started != "hello skipped":
        print(f"\n[hil] confirm_test nohello start hit={started!r}", flush=True)
        sys.exit(5)
    hit2, _ = wait_for(
        fd,
        [
            "Boot confirmation timeout (dry-run)",
        ],
        20,
        buf,
    )
    print(f"\n[hil] confirm_test nohello hit={hit2!r}", flush=True)
    if hit2 != "Boot confirmation timeout (dry-run)":
        sys.exit(5)
    print("[hil] PASS: hello dry-run + timeout dry-run", flush=True)


def run_ota_watch(fd, port, buf, timeout):
    print("--- wait LAN OTA ---", flush=True)
    hit, text = wait_for(fd, NEEDLES_OTA_FAIL + NEEDLES_OTA_OK, timeout, buf)
    print(f"\n[hil] ota hit={hit!r}", flush=True)
    if hit in NEEDLES_OTA_FAIL:
        sys.exit(6)
    if hit is None:
        print("\nTIMEOUT waiting for LAN OTA success", flush=True)
        sys.exit(6)

    print("--- reopen serial after OTA reboot ---", flush=True)
    os.close(fd)
    fd = reopen_serial(port, 40)
    buf.clear()
    # USB-CDC often drops the first boot lines; treat hello-confirm as success
    # even if we missed "pending verification".
    hit, _ = wait_for(
        fd,
        [
            "Boot confirmed via hello (dry-run)",
            "Boot confirmed via hello",
            "Factory boot",
            "OTA boot — pending verification",
            "rolling back",
            "Setup complete",
        ],
        45,
        buf,
    )
    print(f"\n[hil] post-reboot hit={hit!r}", flush=True)
    if hit == "Boot confirmed via hello":
        os.close(fd)
        print("[hil] PASS: LAN OTA + hello confirm", flush=True)
        sys.exit(0)
    if hit in (None, "Factory boot", "Boot confirmed via hello (dry-run)", "rolling back"):
        os.close(fd)
        sys.exit(7)

    print("[hil] real OTA confirm window — waiting for hello confirm", flush=True)
    hit, _ = wait_for(
        fd,
        [
            "Boot confirmed via hello (dry-run)",
            "Boot confirmed via hello",
            "Boot confirmed via button",
            "Boot confirmed via serial",
            "rolling back",
            "Boot confirmation timeout",
        ],
        55,
        buf,
    )
    print(f"\n[hil] confirm hit={hit!r}", flush=True)
    os.close(fd)
    if hit == "Boot confirmed via hello":
        print("[hil] PASS: LAN OTA + hello confirm", flush=True)
        sys.exit(0)
    sys.exit(3)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--skip-nohello", action="store_true")
    parser.add_argument(
        "--mode",
        choices=("dry-run", "heartbeat", "ota-watch", "buttons"),
        default="dry-run",
    )
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument(
        "--expect",
        default="",
        help="Serial substring required in buttons mode (e.g. 'GPIO4 → study')",
    )
    args = parser.parse_args()

    print(f"opening {args.port} mode={args.mode}", flush=True)
    fd = open_serial(args.port)
    buf = []

    if args.mode == "ota-watch":
        run_ota_watch(fd, args.port, buf, args.timeout)
        return

    if args.mode == "buttons":
        expect = args.expect.strip()
        if not expect:
            print("buttons mode requires --expect", flush=True)
            sys.exit(2)
        print(f"--- wait hello buttons {expect!r} ---", flush=True)
        hit, _ = wait_for(fd, (expect,), args.timeout, buf)
        print(f"\n[hil] buttons hit={hit!r}", flush=True)
        os.close(fd)
        if hit is None:
            print("TIMEOUT waiting for hello buttons map", flush=True)
            sys.exit(8)
        print("[hil] PASS: hello buttons map", flush=True)
        sys.exit(0)

    pending, _ = wait_idle(fd, buf)
    if pending:
        print("[hil] real OTA confirm window — waiting for hello confirm", flush=True)
        hit, _ = wait_for(
            fd,
            [
                "Boot confirmed via hello",
                "Boot confirmed via button",
                "Boot confirmed via serial",
                "rolling back",
            ],
            55,
            buf,
        )
        print(f"\n[hil] confirm hit={hit!r}", flush=True)
        os.close(fd)
        sys.exit(0 if hit == "Boot confirmed via hello" else 3)

    if args.mode == "heartbeat":
        buf.clear()
        hit, _ = wait_for(fd, NEEDLES_HEARTBEAT, 20, buf)
        print(f"\n[hil] heartbeat hit={hit!r}", flush=True)
        os.close(fd)
        sys.exit(0 if hit is not None else 2)

    run_dry_run(fd, buf, args.skip_nohello)
    os.close(fd)
    sys.exit(0)


if __name__ == "__main__":
    main()
