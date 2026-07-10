#!/usr/bin/env python3
"""
Sign ESP32 firmware binary with ECDSA (secp256r1) for OTA verification.

Usage:
    python3 sign_firmware.py <firmware.bin> <private_key.pem> [output.sig]

Output:
    <firmware.bin>.sig — 64-byte raw ECDSA signature (r || s, DER-encoded integer pair)
    Or custom output path if specified.
"""

import sys
import os
import hashlib
import subprocess


def sign_firmware(firmware_path: str, key_path: str, sig_path: str) -> None:
    """Hash firmware with SHA-256 and sign with ECDSA private key."""
    
    # Read firmware
    with open(firmware_path, 'rb') as f:
        firmware = f.read()
    
    print(f"Firmware: {firmware_path} ({len(firmware)} bytes)")
    
    # SHA-256 hash
    digest = hashlib.sha256(firmware).digest()
    print(f"SHA-256:  {digest.hex()}")
    
    # Sign with openssl
    # openssl dgst -sha256 -sign key.pem -out sig.der firmware.bin
    result = subprocess.run(
        ['openssl', 'dgst', '-sha256', '-sign', key_path, '-out', sig_path, firmware_path],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"ERROR: OpenSSL signing failed: {result.stderr}")
        sys.exit(1)
    
    # Read the signature (DER format)
    with open(sig_path, 'rb') as f:
        sig_der = f.read()
    
    # Convert DER to raw 64-byte r||s
    # Parse DER SEQUENCE { INTEGER r, INTEGER s }
    if sig_der[0] != 0x30:
        print("ERROR: Expected DER SEQUENCE")
        sys.exit(1)
    
    pos = 2
    if sig_der[1] & 0x80:
        len_bytes = sig_der[1] & 0x7f
        pos += len_bytes
    
    # First INTEGER (r)
    if sig_der[pos] != 0x02:
        print("ERROR: Expected INTEGER for r")
        sys.exit(1)
    pos += 1
    r_len = sig_der[pos]
    pos += 1
    r_bytes = sig_der[pos:pos + r_len]
    pos += r_len
    
    # Second INTEGER (s)
    if sig_der[pos] != 0x02:
        print("ERROR: Expected INTEGER for s")
        sys.exit(1)
    pos += 1
    s_len = sig_der[pos]
    pos += 1
    s_bytes = sig_der[pos:pos + s_len]
    
    # Truncate leading zeros if > 32 bytes, pad if < 32 bytes
    if len(r_bytes) > 32:
        r_bytes = r_bytes[len(r_bytes) - 32:]
    r_padded = b'\x00' * (32 - len(r_bytes)) + r_bytes
    if len(s_bytes) > 32:
        s_bytes = s_bytes[len(s_bytes) - 32:]
    s_padded = b'\x00' * (32 - len(s_bytes)) + s_bytes
    
    raw_sig = r_padded + s_padded
    
    # Write raw signature
    with open(sig_path, 'wb') as f:
        f.write(raw_sig)
    
    print(f"Signature: {sig_path} ({len(raw_sig)} bytes, raw r||s)")
    
    # Verify with public key
    pubkey = subprocess.check_output(
        ['openssl', 'ec', '-in', key_path, '-pubout'],
        stderr=subprocess.DEVNULL
    )
    
    verify = subprocess.run(
        ['openssl', 'dgst', '-sha256', '-verify', '/dev/stdin', '-signature', sig_path, firmware_path],
        input=pubkey,
        capture_output=True,
        text=True
    )
    
    if verify.returncode == 0 and 'Verified OK' in verify.stderr:
        print("Verify:   OK (self-check passed)")
    else:
        print(f"Verify:   FAILED — {verify.stderr.strip()}")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    
    firmware = sys.argv[1]
    key = sys.argv[2]
    sig_output = sys.argv[3] if len(sys.argv) > 3 else firmware + '.sig'
    
    sign_firmware(firmware, key, sig_output)
