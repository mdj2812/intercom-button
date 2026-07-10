#!/bin/bash
# Generate ECDSA secp256r1 key pair for OTA firmware signing.
# Usage: ./generate_ota_keys.sh
# Output: ota_private.pem (private key — KEEP SECRET, DO NOT COMMIT)
#         Prints public key as C header for src/ota_keys.h

set -e
cd "$(dirname "$0")"

echo "Generating secp256r1 ECDSA key pair..."
openssl ecparam -name prime256v1 -genkey -noout -out ota_private.pem
echo "✓ Private key: scripts/ota_private.pem"

echo ""
echo "Public key for src/ota_keys.h:"
echo "---"
python3 << 'PYEOF'
import subprocess, base64, struct

pem = subprocess.check_output(
    ['openssl', 'ec', '-in', 'ota_private.pem', '-pubout'],
    stderr=subprocess.DEVNULL
).decode()

body = ''.join(line.strip() for line in pem.split('\n') if not line.startswith('-----'))
der = base64.b64decode(body)

def read_tag_len(data, pos):
    tag = data[pos]; pos += 1
    length = data[pos]; pos += 1
    if length & 0x80:
        num_len = length & 0x7f
        length = int.from_bytes(data[pos:pos+num_len], 'big')
        pos += num_len
    return tag, length, pos

tag, length, pos = read_tag_len(der, 0)
tag2, length2, pos2 = read_tag_len(der, pos)
pos = pos2 + length2
tag3, length3, pos3 = read_tag_len(der, pos)

unused = der[pos3]
point_data = der[pos3+1:pos3+length3]

print(f'// secp256r1 (NIST P-256) ECDSA public key')
print(f'// Format: uncompressed point (0x04 || X || Y), {len(point_data)} bytes')
print('static const uint8_t OTA_PUBLIC_KEY[] = {')
for i in range(0, len(point_data), 8):
    chunk = point_data[i:i+8]
    hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
    comma = ',' if i + 8 < len(point_data) else ''
    print(f'    {hex_str}{comma}')
print('};')
print(f'static constexpr size_t OTA_PUBLIC_KEY_LEN = {len(point_data)};')
PYEOF

echo "---"
echo "Copy the above into src/ota_keys.h"
echo ""
echo "Keep ota_private.pem SECRET — it's git-ignored. Use it in CI secrets."
