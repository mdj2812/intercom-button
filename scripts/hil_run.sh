#!/usr/bin/env bash
# Hardware-in-the-loop on butler-runner (Gitea `check:host`).
# USB-flashes a baseline image, runs confirm_test, then plants the branch
# firmware on NAS home-intercom and waits for LAN OTA + hello confirm.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOCK="${HIL_LOCK:-/var/lock/intercom-button-hil.lock}"
CONFIG="${HIL_CONFIG:-/opt/intercom-button/hil-config.json}"
LAN_IMAGE="${HIL_LAN_IMAGE:-registry.home.mdj2812.top/intercom-button-dev:latest}"
ESPTOOL=/root/.platformio/packages/tool-esptoolpy/esptool.py
FIRMWARE_ELF_DIR=".pio/build/esp32-s3-devkitc-1"
HIL_MAC="${HIL_MAC:-DC:DA:0C:61:D6:48}"
HIL_SERVER="${HIL_SERVER:-http://192.168.99.10:8764}"
NAS_HOST="${HIL_NAS_HOST:-192.168.99.10}"
NAS_USER="${HIL_NAS_USER:-marmdjtin}"
NAS_KEY="${HIL_NAS_SSH_KEY:-/root/.ssh/id_rsa}"
NAS_DOCKER="${HIL_NAS_DOCKER:-/share/CACHEDEV1_DATA/.qpkg/container-station/bin/docker}"
NAS_CONTAINER="${HIL_NAS_CONTAINER:-home-intercom}"
SKIP_DRY_RUN="${HIL_SKIP_DRY_RUN:-0}"
SKIP_OTA="${HIL_SKIP_OTA:-0}"
USB_FW_VERSION="${HIL_USB_VERSION:-0.2.0}"

exec 9>"$LOCK"
if ! flock -n 9; then
    echo "HIL already running (lock $LOCK)" >&2
    exit 1
fi

USB=""
for d in /dev/ttyACM0 /dev/ttyACM1 /dev/ttyUSB0 /dev/ttyUSB1; do
    if [[ -e "$d" ]]; then
        USB="$d"
        break
    fi
done
if [[ -z "$USB" ]]; then
    echo "No ESP32 serial device (expected /dev/ttyACM* or /dev/ttyUSB*)" >&2
    exit 1
fi
echo "USB: $USB"

if [[ ! -f "$CONFIG" ]]; then
    echo "Missing $CONFIG" >&2
    echo "Copy a LittleFS config (wifi + server) to that path on the runner. See data/config.example.json." >&2
    exit 1
fi

if ! docker image inspect intercom-button-dev:latest >/dev/null 2>&1 &&
    ! docker image inspect ghcr.io/mdj2812/intercom-button-dev:latest >/dev/null 2>&1; then
    echo "Pulling $LAN_IMAGE (avoid ghcr)"
    docker pull "$LAN_IMAGE"
    docker tag "$LAN_IMAGE" intercom-button-dev:latest
    docker tag "$LAN_IMAGE" ghcr.io/mdj2812/intercom-button-dev:latest
fi

mkdir -p data
cp "$CONFIG" data/config.json
chmod 600 data/config.json

wait_usb() {
    local i
    for i in $(seq 1 30); do
        if [[ -e "$USB" ]]; then
            return 0
        fi
        sleep 0.4
    done
    echo "Serial $USB did not reappear" >&2
    return 1
}

read_mac() {
    ./docker/dev.sh python3 "$ESPTOOL" --chip esp32s3 --port "$USB" read_mac \
        | awk 'BEGIN{IGNORECASE=1} /^MAC:/{print toupper($2); exit}'
}

normalize_mac() {
    echo "$1" | tr '[:lower:]' '[:upper:]'
}

set_firmware_version() {
    local ver="$1"
    python3 - "$ver" <<'PY'
from pathlib import Path
import re
import sys
ver = sys.argv[1]
path = Path("src/consts.hpp")
text = path.read_text()
new, n = re.subn(
    r'(constexpr const char\* FIRMWARE_VERSION = ")[^"]+(";)',
    rf"\g<1>{ver}\2",
    text,
    count=1,
)
if n != 1:
    raise SystemExit("could not patch FIRMWARE_VERSION")
path.write_text(new)
print(f"FIRMWARE_VERSION={ver}", flush=True)
PY
}

pio_build() {
    ./docker/dev.sh pio run -e esp32-s3-devkitc-1
}

nas_ssh() {
    ssh -i "$NAS_KEY" -o BatchMode=yes -o ConnectTimeout=8 \
        -o StrictHostKeyChecking=accept-new \
        "${NAS_USER}@${NAS_HOST}" "$@"
}

manage() {
    local action="$1"
    curl -sS -m 120 -H "Content-Type: application/json" \
        -d "{\"mac\":\"$BOARD_MAC\",\"action\":\"$action\"}" \
        "$HIL_SERVER/api/home_intercom/devices/manage"
}

BACKUP_ON_NAS=""
restore_firmware_cache() {
    if [[ -z "$BACKUP_ON_NAS" ]]; then
        return 0
    fi
    echo "=== restore NAS firmware cache ==="
    nas_ssh "export PATH=$(dirname "$NAS_DOCKER"):\$PATH
set -e
B=$BACKUP_ON_NAS
docker cp \$B/firmware.bin $NAS_CONTAINER:/data/firmware/firmware.bin
docker cp \$B/firmware.json $NAS_CONTAINER:/data/firmware/firmware.json
if [ -f \$B/firmware.sig ]; then
  docker cp \$B/firmware.sig $NAS_CONTAINER:/data/firmware/firmware.sig
else
  docker exec -u root $NAS_CONTAINER rm -f /data/firmware/firmware.sig
fi
rm -rf \$B" || echo "WARNING: failed to restore NAS firmware cache" >&2
    BACKUP_ON_NAS=""
}

trap restore_firmware_cache EXIT

CONSTS_ORIG=""
restore_consts() {
    if [[ -n "$CONSTS_ORIG" && -f "$CONSTS_ORIG" ]]; then
        cp "$CONSTS_ORIG" src/consts.hpp
        rm -f "$CONSTS_ORIG"
        CONSTS_ORIG=""
    fi
}
trap 'restore_consts; restore_firmware_cache' EXIT

echo "=== read MAC ==="
wait_usb
BOARD_MAC="$(normalize_mac "$(read_mac)")"
echo "MAC: $BOARD_MAC"
if [[ -z "$BOARD_MAC" ]]; then
    echo "Could not read ESP32 MAC" >&2
    exit 1
fi
EXPECTED="$(normalize_mac "$HIL_MAC")"
if [[ "$BOARD_MAC" != "$EXPECTED" ]]; then
    echo "Refusing to flash $BOARD_MAC (reserved HIL MAC is $EXPECTED)" >&2
    exit 1
fi

OTA_VERSION="$(sed -n 's/.*FIRMWARE_VERSION = "\([^"]*\)".*/\1/p' src/consts.hpp | head -1)"
if [[ -z "$OTA_VERSION" ]]; then
    echo "Could not read FIRMWARE_VERSION" >&2
    exit 1
fi
if [[ "$SKIP_OTA" != "1" && "$USB_FW_VERSION" == "$OTA_VERSION" ]]; then
    echo "HIL_USB_VERSION ($USB_FW_VERSION) must differ from $OTA_VERSION so hello returns ota:true" >&2
    exit 1
fi

echo "=== build OTA image ($OTA_VERSION) ==="
pio_build
cp "$FIRMWARE_ELF_DIR/firmware.bin" /tmp/hil-ota-firmware.bin
OTA_SHA="$(sha256sum /tmp/hil-ota-firmware.bin | awk '{print $1}')"
OTA_SIZE="$(wc -c < /tmp/hil-ota-firmware.bin | tr -d ' ')"
echo "OTA bin $OTA_SIZE bytes sha256=$OTA_SHA"

CONSTS_ORIG="$(mktemp)"
cp src/consts.hpp "$CONSTS_ORIG"
echo "=== build USB baseline ($USB_FW_VERSION) ==="
set_firmware_version "$USB_FW_VERSION"
pio_build

echo "=== flash factory ==="
./docker/dev.sh pio run -e esp32-s3-devkitc-1 -t upload
wait_usb

echo "=== clear otadata (boot factory slot) ==="
./docker/dev.sh python3 "$ESPTOOL" --chip esp32s3 --port "$USB" \
    --before default_reset --after hard_reset erase_region 0xe000 0x2000
wait_usb

echo "=== upload LittleFS ==="
./docker/dev.sh pio run -e esp32-s3-devkitc-1 -t uploadfs
wait_usb
sleep 2
restore_consts

if [[ "$SKIP_DRY_RUN" != "1" ]]; then
    echo "=== serial confirm_test ==="
    python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB"
fi

if [[ "$SKIP_OTA" == "1" ]]; then
    echo "HIL OK (USB + dry-run; LAN OTA skipped)"
    exit 0
fi

echo "=== approve $BOARD_MAC ==="
echo "$(manage approve)"

echo "=== stage OTA bin on NAS ==="
scp -i "$NAS_KEY" -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
    /tmp/hil-ota-firmware.bin "${NAS_USER}@${NAS_HOST}:/tmp/hil-ota-firmware.bin"
python3 - "$OTA_VERSION" "$OTA_SHA" <<'PY' > /tmp/hil-ota-firmware.json
import json, sys
print(json.dumps({
    "version": sys.argv[1],
    "sha256": sys.argv[2],
    "tag": "hil-ota",
    "asset": "firmware.bin",
}, indent=2))
PY
scp -i "$NAS_KEY" -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
    /tmp/hil-ota-firmware.json "${NAS_USER}@${NAS_HOST}:/tmp/hil-ota-firmware.json"

echo "=== backup NAS firmware cache ==="
BACKUP_ON_NAS="/tmp/hil-fw-backup"
nas_ssh "export PATH=$(dirname "$NAS_DOCKER"):\$PATH
set -e
rm -rf $BACKUP_ON_NAS
mkdir -p $BACKUP_ON_NAS
docker cp $NAS_CONTAINER:/data/firmware/firmware.bin $BACKUP_ON_NAS/firmware.bin
docker cp $NAS_CONTAINER:/data/firmware/firmware.json $BACKUP_ON_NAS/firmware.json
docker cp $NAS_CONTAINER:/data/firmware/firmware.sig $BACKUP_ON_NAS/firmware.sig 2>/dev/null || true"

echo "=== wait hello heartbeat (10s OTA window) ==="
python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB" --mode heartbeat

plant_failed=0
echo "=== POST ota (GitHub fetch, then plant branch bin) ==="
OTA_RESP="$(manage ota)" || plant_failed=1
echo "$OTA_RESP"
if [[ "$plant_failed" != "0" ]]; then
    echo "manage ota failed" >&2
    manage deapprove >/dev/null || true
    exit 1
fi

echo "=== plant branch firmware (unsigned) ==="
if ! nas_ssh "export PATH=$(dirname "$NAS_DOCKER"):\$PATH
docker cp /tmp/hil-ota-firmware.bin $NAS_CONTAINER:/data/firmware/firmware.bin
docker cp /tmp/hil-ota-firmware.json $NAS_CONTAINER:/data/firmware/firmware.json
docker exec -u root $NAS_CONTAINER rm -f /data/firmware/firmware.sig
rm -f /tmp/hil-ota-firmware.bin /tmp/hil-ota-firmware.json"; then
    echo "plant failed — deapprove so the board does not flash GitHub v$OTA_VERSION" >&2
    manage deapprove >/dev/null || true
    exit 1
fi

echo "=== serial watch LAN OTA ==="
set +e
python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB" --mode ota-watch --timeout 180
WATCH_RC=$?
set -e
if [[ "$WATCH_RC" -ne 0 ]]; then
    echo "LAN OTA watch failed (rc=$WATCH_RC)" >&2
    exit "$WATCH_RC"
fi

echo "HIL OK"
