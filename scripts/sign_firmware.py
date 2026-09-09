#!/usr/bin/env python3
"""
Sign ESP32 firmware binary with ECDSA (secp256r1) for OTA verification.

Usage:
    python3 sign_firmware.py <firmware.bin> <private_key.pem> [output.sig]

Output:
    <firmware.bin>.sig — 64-byte raw ECDSA signature (r || s, 32 bytes each)
    Or custom output path if specified.
"""

from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path


def _der_to_raw_rs(sig_der: bytes) -> bytes:
    """Convert OpenSSL ECDSA DER SEQUENCE {INTEGER r, INTEGER s} to raw r||s."""
    if not sig_der or sig_der[0] != 0x30:
        raise ValueError("expected DER SEQUENCE")

    pos = 2
    if sig_der[1] & 0x80:
        pos += sig_der[1] & 0x7F

    if sig_der[pos] != 0x02:
        raise ValueError("expected INTEGER for r")
    pos += 1
    r_len = sig_der[pos]
    pos += 1
    r_bytes = sig_der[pos : pos + r_len]
    pos += r_len

    if sig_der[pos] != 0x02:
        raise ValueError("expected INTEGER for s")
    pos += 1
    s_len = sig_der[pos]
    pos += 1
    s_bytes = sig_der[pos : pos + s_len]

    if len(r_bytes) > 32:
        r_bytes = r_bytes[-32:]
    if len(s_bytes) > 32:
        s_bytes = s_bytes[-32:]
    r_padded = b"\x00" * (32 - len(r_bytes)) + r_bytes
    s_padded = b"\x00" * (32 - len(s_bytes)) + s_bytes
    return r_padded + s_padded


def sign_firmware(firmware_path: str, key_path: str, sig_path: str) -> None:
    """Hash firmware with SHA-256 and sign with ECDSA private key."""
    firmware = Path(firmware_path).read_bytes()
    print(f"Firmware: {firmware_path} ({len(firmware)} bytes)")
    digest = hashlib.sha256(firmware).digest()
    print(f"SHA-256:  {digest.hex()}")

    der_path = sig_path + ".der.tmp"
    pub_path = sig_path + ".pub.tmp"
    try:
        result = subprocess.run(
            ["openssl", "dgst", "-sha256", "-sign", key_path, "-out", der_path, firmware_path],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"ERROR: OpenSSL signing failed: {result.stderr}")
            sys.exit(1)

        sig_der = Path(der_path).read_bytes()
        try:
            raw_sig = _der_to_raw_rs(sig_der)
        except ValueError as exc:
            print(f"ERROR: {exc}")
            sys.exit(1)
        if len(raw_sig) != 64:
            print(f"ERROR: raw signature is {len(raw_sig)} bytes, want 64")
            sys.exit(1)

        Path(sig_path).write_bytes(raw_sig)
        print(f"Signature: {sig_path} ({len(raw_sig)} bytes, raw r||s)")

        pubkey = subprocess.check_output(
            ["openssl", "ec", "-in", key_path, "-pubout"],
            stderr=subprocess.DEVNULL,
        )
        Path(pub_path).write_bytes(pubkey)
        verify = subprocess.run(
            [
                "openssl",
                "dgst",
                "-sha256",
                "-verify",
                pub_path,
                "-signature",
                der_path,
                firmware_path,
            ],
            capture_output=True,
            text=True,
        )
        if verify.returncode != 0:
            print(f"Verify:   FAILED — {(verify.stderr or verify.stdout).strip()}")
            sys.exit(1)
        print("Verify:   OK (OpenSSL self-check on DER)")
    finally:
        for tmp in (der_path, pub_path):
            try:
                os.remove(tmp)
            except FileNotFoundError:
                pass


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    firmware = sys.argv[1]
    key = sys.argv[2]
    sig_output = sys.argv[3] if len(sys.argv) > 3 else firmware + ".sig"
    sign_firmware(firmware, key, sig_output)
