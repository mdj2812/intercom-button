#!/usr/bin/env bash
# Hardware-in-the-loop on butler-runner (Gitea `check:host`).
# Flashes factory, clears OTA data, uploads LittleFS, runs confirm_test.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LOCK="${HIL_LOCK:-/var/lock/intercom-button-hil.lock}"
CONFIG="${HIL_CONFIG:-/opt/intercom-button/hil-config.json}"
LAN_IMAGE="${HIL_LAN_IMAGE:-registry.home.mdj2812.top/intercom-button-dev:latest}"
ESPTOOL=/root/.platformio/packages/tool-esptoolpy/esptool.py

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

echo "=== build ==="
./docker/dev.sh pio run -e esp32-s3-devkitc-1

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

echo "=== serial confirm_test ==="
python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB"
echo "HIL OK"
