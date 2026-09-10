#!/usr/bin/env python3
"""Serial smoke test for the OTA confirm dry-run (issue #42 / #44)."""
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
    "Room map from server",
    "Waiting for approval",
    "Hello failed",
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--skip-nohello", action="store_true")
    args = parser.parse_args()

    print(f"opening {args.port}", flush=True)
    fd = open_serial(args.port)
    buf = []
    print("--- wait idle ---", flush=True)
    hit, text = wait_for(fd, NEEDLES_IDLE, 45, buf)
    if hit is None:
        print("\nTIMEOUT waiting for boot/idle log", flush=True)
        sys.exit(2)

    pending = "pending verification" in text or "OTA boot" in text
    print(f"\n[hil] boot hit={hit!r} pending_verify={pending}", flush=True)

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

    time.sleep(1)
    read_more(fd, 0.5, buf)
    buf.clear()
    send(fd, "confirm_test")
    started, _ = wait_for(fd, ["OTA confirm dry-run", "Unknown command", "confirm_test only from idle"], 20, buf)
    if started != "OTA confirm dry-run":
        print(f"\n[hil] confirm_test start hit={started!r}", flush=True)
        os.close(fd)
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
        os.close(fd)
        sys.exit(4)

    if args.skip_nohello:
        os.close(fd)
        print("[hil] PASS: hello dry-run", flush=True)
        sys.exit(0)

    # Wait until IDLE is reading serial again (a blocking hello can eat the next command).
    time.sleep(2)
    read_more(fd, 1.0, buf)
    buf.clear()
    send(fd, "confirm_test nohello")
    started, _ = wait_for(fd, ["hello skipped", "Unknown command"], 25, buf)
    if started != "hello skipped":
        print(f"\n[hil] confirm_test nohello start hit={started!r}", flush=True)
        os.close(fd)
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
    os.close(fd)
    if hit2 != "Boot confirmation timeout (dry-run)":
        sys.exit(5)
    print("[hil] PASS: hello dry-run + timeout dry-run", flush=True)
    sys.exit(0)


if __name__ == "__main__":
    main()
