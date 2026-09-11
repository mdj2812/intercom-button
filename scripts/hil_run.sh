#!/usr/bin/env bash
# Hardware-in-the-loop on butler-runner (Gitea `check:host`).
# Phases: compile | flash | test | all (default).
# USB-flashes a baseline image, runs confirm_test, then plants a *signed*
# branch firmware on the LAN server (ECDSA + X-Checksum-SHA256). A tampered
# .sig and checksum must fail before the good image boots and hello-confirms.
#
# Gitea Actions (`.gitea/workflows/hil.yml`) passes repo variables into these
# HIL_* env vars. There are no lab defaults in this file — unset required
# vars fail the job. Wifi stays in HIL_CONFIG on the runner (not a variable).
# SSH uses the runner's default identity (optional HIL_SSH_KEY for -i).
# `docker` on the SSH host: set HIL_DOCKER on the runner if it is not on PATH.
# Signing: Gitea secret OTA_PRIVATE_KEY (same PEM as GitHub) or HIL_OTA_KEY path.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

usage() {
    cat <<'EOF'
Usage: scripts/hil_run.sh [compile|flash|test|all]

  compile  Build OTA + USB images, sign the OTA bin, LittleFS (no USB)
  flash    Write compile artifacts to the reserved ESP32
  test     Serial dry-run, signed LAN OTA (tamper + good), hello buttons
  all      compile + flash + test (default)
EOF
}

PHASE="${1:-all}"
case "$PHASE" in
    compile | flash | test | all) ;;
    -h | --help | help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac

LOCK="${HIL_LOCK:-/var/lock/intercom-button-hil.lock}"
CONFIG="${HIL_CONFIG:-/opt/intercom-button/hil-config.json}"
LAN_IMAGE="${HIL_LAN_IMAGE:-}"
ESPTOOL=/root/.platformio/packages/tool-esptoolpy/esptool.py
FIRMWARE_ELF_DIR=".pio/build/esp32-s3-devkitc-1"
HIL_MAC="${HIL_MAC:-}"
HIL_SERVER="${HIL_SERVER:-}"
HIL_SSH_HOST="${HIL_SSH_HOST:-}"
HIL_SSH_USER="${HIL_SSH_USER:-}"
HIL_CONTAINER="${HIL_CONTAINER:-}"
SKIP_DRY_RUN="${HIL_SKIP_DRY_RUN:-0}"
SKIP_OTA="${HIL_SKIP_OTA:-0}"
SKIP_TAMPER="${HIL_SKIP_TAMPER:-0}"
SKIP_BUTTONS="${HIL_SKIP_BUTTONS:-0}"
USB_FW_VERSION="${HIL_USB_VERSION:-0.2.0}"
LITTLEFS_OFFSET="${HIL_LITTLEFS_OFFSET:-0x610000}"

if [[ -z "$LAN_IMAGE" && -f "$ROOT/docker/.docker-image" ]]; then
    LAN_IMAGE="$(head -1 "$ROOT/docker/.docker-image" | tr -d '\n\r')"
fi

require_vars() {
    local missing=() n
    for n in "$@"; do
        if [[ -z "${!n:-}" ]]; then
            missing+=("$n")
        fi
    done
    if ((${#missing[@]})); then
        echo "Missing required HIL env: ${missing[*]}" >&2
        echo "Set them as Gitea Actions variables (or export them for a local run)." >&2
        exit 1
    fi
}

case "$PHASE" in
    flash) require_vars HIL_MAC ;;
    test | all) require_vars HIL_MAC HIL_SERVER HIL_SSH_HOST HIL_SSH_USER HIL_CONTAINER ;;
esac
SSH_HOST="$HIL_SSH_HOST"
SSH_USER="$HIL_SSH_USER"
CONTAINER="$HIL_CONTAINER"

if [[ -n "${HIL_ARTIFACT_DIR:-}" ]]; then
    ARTIFACT_DIR="$HIL_ARTIFACT_DIR"
elif [[ -n "${GITHUB_RUN_ID:-}" ]]; then
    ARTIFACT_DIR="/var/tmp/intercom-button-hil/${GITHUB_RUN_ID}"
else
    ARTIFACT_DIR="$ROOT/.hil"
fi
WS_ARTIFACT_DIR="$ROOT/.hil"
META_SH="$ARTIFACT_DIR/meta.sh"

need_lock=0
need_usb=0
need_config=0
case "$PHASE" in
    all | flash | test) need_lock=1 ;;
esac
case "$PHASE" in
    all | flash | test) need_usb=1 ;;
esac
case "$PHASE" in
    all | compile) need_config=1 ;;
esac

if [[ "$need_lock" -eq 1 ]]; then
    exec 9>"$LOCK"
    if ! flock -n 9; then
        echo "HIL already running (lock $LOCK)" >&2
        exit 1
    fi
fi

USB=""
if [[ "$need_usb" -eq 1 ]]; then
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
fi

if [[ "$need_config" -eq 1 && ! -f "$CONFIG" ]]; then
    echo "Missing $CONFIG" >&2
    echo "Copy a LittleFS config (wifi + server) to that path on the runner. See data/config.example.json." >&2
    exit 1
fi

ensure_image() {
    if ! docker image inspect intercom-button-dev:latest >/dev/null 2>&1 &&
        ! docker image inspect ghcr.io/mdj2812/intercom-button-dev:latest >/dev/null 2>&1; then
        if [[ -z "$LAN_IMAGE" ]]; then
            echo "Set HIL_LAN_IMAGE (or docker/.docker-image) to pull a build image." >&2
            exit 1
        fi
        echo "Pulling $LAN_IMAGE"
        docker pull "$LAN_IMAGE"
        docker tag "$LAN_IMAGE" intercom-button-dev:latest
        docker tag "$LAN_IMAGE" ghcr.io/mdj2812/intercom-button-dev:latest
    fi
}

sync_artifacts_to_workspace() {
    mkdir -p "$WS_ARTIFACT_DIR"
    local src dst
    src="$(cd "$ARTIFACT_DIR" && pwd)"
    dst="$(cd "$WS_ARTIFACT_DIR" && pwd)"
    if [[ "$src" != "$dst" ]]; then
        cp -a "$ARTIFACT_DIR/." "$WS_ARTIFACT_DIR/"
    fi
}

load_meta() {
    if [[ ! -f "$META_SH" ]]; then
        echo "Missing compile artifacts ($META_SH). Run compile first." >&2
        exit 1
    fi
    # shellcheck disable=SC1090
    source "$META_SH"
}

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
        | tr -d '\r' \
        | awk 'BEGIN{IGNORECASE=1} /^MAC:/{print toupper($2); exit}'
}

normalize_mac() {
    # esptool/docker often emit CR; Gitea logs turn that into a false mismatch.
    echo "$1" | tr -d '\r' | tr '[:lower:]' '[:upper:]' | tr -d '[:space:]'
}

require_hil_mac() {
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

export_usb_flash_layout() {
    python3 - "$FIRMWARE_ELF_DIR" "$ARTIFACT_DIR/usb" <<'PY'
import json, shutil, sys
from pathlib import Path

build, dest = Path(sys.argv[1]), Path(sys.argv[2])
dest.mkdir(parents=True, exist_ok=True)
files = {
    "0x0": build / "bootloader.bin",
    "0x8000": build / "partitions.bin",
    "0x10000": build / "firmware.bin",
}
settings = ["--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "8MB"]
meta = build / "flasher_args.json"
if meta.exists():
    data = json.loads(meta.read_text())
    flash_settings = data.get("flash_settings") or {}
    settings = [
        "--flash_mode",
        flash_settings.get("flash_mode", "dio"),
        "--flash_freq",
        flash_settings.get("flash_freq", "80m"),
        "--flash_size",
        flash_settings.get("flash_size", "8MB"),
    ]
    raw = data.get("flash_files") or {}
    if raw:
        files = {}
        for offset, rel in raw.items():
            path = Path(rel)
            if not path.is_absolute():
                path = build / path
            files[offset] = path
for offset, path in list(files.items()):
    if not path.exists() and path.name == "bootloader.bin":
        alt = build / "bootloader" / "bootloader.bin"
        if alt.exists():
            files[offset] = alt
missing = [str(path) for path in files.values() if not path.exists()]
if missing:
    listed = " ".join(sorted(p.name for p in build.glob("*") if p.is_file()))
    raise SystemExit(f"missing flash files: {', '.join(missing)} (in {build}: {listed})")
lines = [" ".join(settings)]
for offset, path in files.items():
    shutil.copy2(path, dest / path.name)
    lines.append(f"{offset} {path.name}")
(dest / "flash_args").write_text("\n".join(lines) + "\n")
print(f"flash_args -> {dest / 'flash_args'}", flush=True)
PY
}

esptool() {
    ./docker/dev.sh python3 "$ESPTOOL" "$@"
}

hil_ssh() {
    local -a cmd=(ssh -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new)
    if [[ -n "${HIL_SSH_KEY:-}" ]]; then
        cmd+=(-i "$HIL_SSH_KEY")
    fi
    "${cmd[@]}" "${SSH_USER}@${SSH_HOST}" "$@"
}

hil_scp() {
    local -a cmd=(scp -o BatchMode=yes -o StrictHostKeyChecking=accept-new)
    if [[ -n "${HIL_SSH_KEY:-}" ]]; then
        cmd+=(-i "$HIL_SSH_KEY")
    fi
    "${cmd[@]}" "$@"
}

# If the runner set an absolute `docker` path, put that directory on PATH.
docker_path() {
    if [[ "${HIL_DOCKER:-}" == /* ]]; then
        printf 'export PATH=%q:"$PATH"\n' "$(dirname "$HIL_DOCKER")"
    fi
}

manage() {
    local action="$1"
    curl -sS -m 120 -H "Content-Type: application/json" \
        -d "{\"mac\":\"$BOARD_MAC\",\"action\":\"$action\"}" \
        "$HIL_SERVER/api/home_intercom/devices/manage"
}

manage_buttons() {
    local buttons_json="$1"
    python3 - "$HIL_SERVER" "$BOARD_MAC" "$buttons_json" <<'PY'
import json, sys, urllib.error, urllib.request
server, mac, buttons = sys.argv[1].rstrip("/"), sys.argv[2], json.loads(sys.argv[3])
body = json.dumps({"mac": mac, "action": "buttons", "buttons": buttons}).encode()
req = urllib.request.Request(
    server + "/api/home_intercom/devices/manage",
    data=body,
    headers={"Content-Type": "application/json"},
    method="POST",
)
try:
    with urllib.request.urlopen(req, timeout=120) as resp:
        print(resp.read().decode())
except urllib.error.HTTPError as exc:
    print(exc.read().decode(), file=sys.stderr)
    raise
PY
}

hil_buttons_plan() {
    python3 - "$HIL_SERVER" "$BOARD_MAC" <<'PY'
import json, sys, urllib.request
server, mac = sys.argv[1].rstrip("/"), sys.argv[2].upper().replace("-", ":")
rooms = json.load(urllib.request.urlopen(server + "/api/home_intercom/rooms", timeout=15))
if not isinstance(rooms, dict) or not rooms:
    raise SystemExit("room catalog is empty")
keys = list(rooms)
devices = json.load(urllib.request.urlopen(server + "/api/home_intercom/devices", timeout=15))
prev = {}
for key, device in devices.items():
    if str(key).upper().replace("-", ":") == mac:
        prev = device.get("buttons") or {}
        break
want = keys[0]
current = str(prev.get("4") or "")
if current == want and len(keys) > 1:
    want = keys[1]
print(json.dumps({"prev": prev, "want": want, "apply": {"4": want}}, separators=(",", ":")))
PY
}

write_ota_json() {
    local sha="$1"
    local dest="$2"
    python3 - "$OTA_VERSION" "$sha" <<'PY' >"$dest"
import json, sys
print(json.dumps({
    "version": sys.argv[1],
    "sha256": sys.argv[2],
    "tag": "hil-ota",
    "asset": "firmware.bin",
}, indent=2))
PY
}

assert_ota_key_matches_firmware() {
    local key="$1"
    python3 - "$key" "$ROOT/src/ota_keys.h" <<'PY'
import re, subprocess, sys
from pathlib import Path

key_path, header_path = sys.argv[1], sys.argv[2]
text = Path(header_path).read_text()
chunk = text.split("OTA_PUBLIC_KEY[]", 1)[1].split(";", 1)[0]
want = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", chunk))
if len(want) != 65:
    raise SystemExit(f"{header_path} public key is {len(want)} bytes, want 65")
der = subprocess.check_output(
    ["openssl", "ec", "-in", key_path, "-pubout", "-conv_form", "uncompressed", "-outform", "DER"],
    stderr=subprocess.DEVNULL,
)
if want not in der:
    raise SystemExit("OTA_PRIVATE_KEY / HIL_OTA_KEY does not match src/ota_keys.h")
print("OTA signing key matches src/ota_keys.h", flush=True)
PY
}

sign_ota_bin() {
    local key sig n
    if ! command -v openssl >/dev/null 2>&1; then
        echo "openssl is required to sign the HIL OTA image" >&2
        exit 1
    fi
    key="$(mktemp)"
    chmod 600 "$key"
    if [[ -n "${HIL_OTA_KEY:-}" ]]; then
        if [[ ! -f "$HIL_OTA_KEY" ]]; then
            echo "HIL_OTA_KEY is not a file: $HIL_OTA_KEY" >&2
            exit 1
        fi
        cp "$HIL_OTA_KEY" "$key"
    elif [[ -n "${OTA_PRIVATE_KEY:-}" ]]; then
        printf '%s\n' "$OTA_PRIVATE_KEY" | tr -d '\r' >"$key"
    else
        echo "Need Gitea secret OTA_PRIVATE_KEY (same PEM as GitHub) or HIL_OTA_KEY (PEM path) to sign the HIL OTA image." >&2
        exit 1
    fi
    assert_ota_key_matches_firmware "$key"
    sig="$ARTIFACT_DIR/ota/firmware.sig"
    python3 "$ROOT/scripts/sign_firmware.py" "$ARTIFACT_DIR/ota/firmware.bin" "$key" "$sig"
    rm -f "$key"
    n="$(wc -c <"$sig" | tr -d ' ')"
    if [[ "$n" != "64" ]]; then
        echo "firmware.sig is $n bytes, want 64" >&2
        exit 1
    fi
}

stage_ota_files() {
    hil_scp "$ARTIFACT_DIR/ota/firmware.bin" "${SSH_USER}@${SSH_HOST}:/tmp/hil-ota-firmware.bin"
    hil_scp "$ARTIFACT_DIR/ota/firmware.json" "${SSH_USER}@${SSH_HOST}:/tmp/hil-ota-firmware.json"
    hil_scp "$ARTIFACT_DIR/ota/firmware.sig" "${SSH_USER}@${SSH_HOST}:/tmp/hil-ota-firmware.sig"
    if [[ -f "$ARTIFACT_DIR/ota/firmware.bad.json" ]]; then
        hil_scp "$ARTIFACT_DIR/ota/firmware.bad.json" "${SSH_USER}@${SSH_HOST}:/tmp/hil-ota-firmware.bad.json"
    fi
    if [[ -f "$ARTIFACT_DIR/ota/firmware.bad.sig" ]]; then
        hil_scp "$ARTIFACT_DIR/ota/firmware.bad.sig" "${SSH_USER}@${SSH_HOST}:/tmp/hil-ota-firmware.bad.sig"
    fi
}

# json_remote / sig_remote are filenames already on the NAS at /tmp/hil-ota-*.
# Only docker cp (no scp) so this can run inside the 10s hello OTA window.
apply_ota_cache() {
    local json_remote="$1"
    local sig_remote="${2:-}"
    local have_sig=0
    [[ -n "$sig_remote" ]] && have_sig=1
    hil_ssh "$(docker_path)
set -e
HAVE_SIG=$have_sig
JSON=$json_remote
SIG=$sig_remote
docker cp /tmp/hil-ota-firmware.bin $CONTAINER:/data/firmware/firmware.bin
docker cp /tmp/hil-ota-\$JSON $CONTAINER:/data/firmware/firmware.json
if [ \$HAVE_SIG = 1 ]; then
  docker cp /tmp/hil-ota-\$SIG $CONTAINER:/data/firmware/firmware.sig
else
  docker exec -u root $CONTAINER rm -f /data/firmware/firmware.sig
fi"
}

verify_firmware_http() {
    local expect_sha="$1"
    local expect_sig_len="$2"
    python3 - "$HIL_SERVER" "$expect_sha" "$expect_sig_len" <<'PY'
import sys, urllib.error, urllib.request

server, expect_sha, expect_sig_len = sys.argv[1].rstrip("/"), sys.argv[2].lower(), int(sys.argv[3])
fw_url = server + "/api/home_intercom/firmware"
sig_url = server + "/api/home_intercom/firmware.sig"

req = urllib.request.Request(fw_url, method="HEAD")
sha = ""
size = None
try:
    with urllib.request.urlopen(req, timeout=30) as resp:
        headers = {k.lower(): v for k, v in resp.headers.items()}
        sha = (headers.get("x-checksum-sha256") or "").strip().lower()
        size = resp.headers.get("Content-Length")
except urllib.error.HTTPError:
    sha = ""
if not sha:
    with urllib.request.urlopen(fw_url, timeout=60) as resp:
        headers = {k.lower(): v for k, v in resp.headers.items()}
        sha = (headers.get("x-checksum-sha256") or "").strip().lower()
        body = resp.read()
        size = str(len(body))
if sha != expect_sha:
    raise SystemExit(f"X-Checksum-SHA256 {sha!r} != {expect_sha!r}")
print(f"GET /firmware: {size or '?'} bytes X-Checksum-SHA256={sha}", flush=True)

try:
    with urllib.request.urlopen(sig_url, timeout=30) as resp:
        sig_body = resp.read()
except urllib.error.HTTPError as exc:
    if expect_sig_len == 0 and exc.code == 404:
        print("GET /firmware.sig: 404 (expected)", flush=True)
        raise SystemExit(0)
    raise SystemExit(f"GET /firmware.sig failed: HTTP {exc.code}") from exc
if len(sig_body) != expect_sig_len:
    raise SystemExit(f"firmware.sig is {len(sig_body)} bytes, want {expect_sig_len}")
print(f"GET /firmware.sig: {len(sig_body)} bytes", flush=True)
PY
}

confirm_test() {
    python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB" "$@"
}

FW_BACKUP=""
HIL_BUTTONS_RESTORE=""
restore_firmware_cache() {
    if [[ -z "$FW_BACKUP" ]]; then
        return 0
    fi
    echo "=== restore firmware cache ==="
    hil_ssh "$(docker_path)
set -e
B=$FW_BACKUP
docker cp \$B/firmware.bin $CONTAINER:/data/firmware/firmware.bin
docker cp \$B/firmware.json $CONTAINER:/data/firmware/firmware.json
if [ -f \$B/firmware.sig ]; then
  docker cp \$B/firmware.sig $CONTAINER:/data/firmware/firmware.sig
else
  docker exec -u root $CONTAINER rm -f /data/firmware/firmware.sig
fi
rm -rf \$B
rm -f /tmp/hil-ota-firmware.bin /tmp/hil-ota-firmware.json /tmp/hil-ota-firmware.sig \
      /tmp/hil-ota-firmware.bad.json /tmp/hil-ota-firmware.bad.sig" || echo "WARNING: failed to restore firmware cache" >&2
    FW_BACKUP=""
}

restore_hil_buttons() {
    if [[ -z "${HIL_BUTTONS_RESTORE}" ]]; then
        return 0
    fi
    echo "=== restore HIL GPIO map ==="
    manage_buttons "$HIL_BUTTONS_RESTORE" || echo "WARNING: failed to restore HIL GPIO map" >&2
    HIL_BUTTONS_RESTORE=""
}

CONSTS_ORIG=""
restore_consts() {
    if [[ -n "$CONSTS_ORIG" && -f "$CONSTS_ORIG" ]]; then
        cp "$CONSTS_ORIG" src/consts.hpp
        rm -f "$CONSTS_ORIG"
        CONSTS_ORIG=""
    fi
}

cleanup_ci_artifacts() {
    case "$ARTIFACT_DIR" in
        /var/tmp/intercom-button-hil/*)
            rm -rf "$ARTIFACT_DIR"
            ;;
    esac
}

on_exit() {
    restore_consts
    restore_hil_buttons
    restore_firmware_cache
    if [[ "$PHASE" == "test" || "$PHASE" == "all" ]]; then
        cleanup_ci_artifacts
    fi
}
trap on_exit EXIT

do_compile() {
    echo "=== compile ($PHASE) ==="
    ensure_image
    mkdir -p "$ARTIFACT_DIR/usb" "$ARTIFACT_DIR/ota" data
    cp "$CONFIG" data/config.json
    chmod 600 data/config.json

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
    cp "$FIRMWARE_ELF_DIR/firmware.bin" "$ARTIFACT_DIR/ota/firmware.bin"
    OTA_SHA="$(sha256sum "$ARTIFACT_DIR/ota/firmware.bin" | awk '{print $1}')"
    OTA_SIZE="$(wc -c < "$ARTIFACT_DIR/ota/firmware.bin" | tr -d ' ')"
    echo "OTA bin $OTA_SIZE bytes sha256=$OTA_SHA"
    if [[ "$SKIP_OTA" != "1" ]]; then
        echo "=== sign OTA image ==="
        sign_ota_bin
    fi

    CONSTS_ORIG="$(mktemp)"
    cp src/consts.hpp "$CONSTS_ORIG"
    echo "=== build USB baseline ($USB_FW_VERSION) ==="
    set_firmware_version "$USB_FW_VERSION"
    pio_build
    export_usb_flash_layout

    echo "=== build LittleFS ==="
    ./docker/dev.sh pio run -e esp32-s3-devkitc-1 -t buildfs
    if [[ -f "$FIRMWARE_ELF_DIR/littlefs.bin" ]]; then
        cp "$FIRMWARE_ELF_DIR/littlefs.bin" "$ARTIFACT_DIR/usb/littlefs.bin"
    elif [[ -f "$FIRMWARE_ELF_DIR/spiffs.bin" ]]; then
        cp "$FIRMWARE_ELF_DIR/spiffs.bin" "$ARTIFACT_DIR/usb/littlefs.bin"
    else
        echo "LittleFS image not produced" >&2
        ls -la "$FIRMWARE_ELF_DIR" >&2 || true
        exit 1
    fi
    restore_consts

    cat >"$META_SH" <<EOF
OTA_VERSION=$(printf '%q' "$OTA_VERSION")
OTA_SHA=$(printf '%q' "$OTA_SHA")
OTA_SIZE=$(printf '%q' "$OTA_SIZE")
USB_FW_VERSION=$(printf '%q' "$USB_FW_VERSION")
EOF
    echo "Artifacts: $ARTIFACT_DIR"
}

# Pending hello does not start OTA (should_start_ota only on status ok).
# Leftover ota_requested (PWA Update) still fires after approve, so also cancel it.
hold_hello_pending() {
    BOARD_MAC="$(normalize_mac "$HIL_MAC")"
    echo "=== deapprove $BOARD_MAC (no hello ota until after confirm_test) ==="
    echo "$(manage deapprove)"
    cancel_leftover_ota
}

cancel_leftover_ota() {
    echo "=== cancel leftover ota $BOARD_MAC ==="
    local resp
    resp="$(manage ota_cancel)"
    echo "$resp"
    if echo "$resp" | grep -q '"ota_requested":false'; then
        return 0
    fi
    set +e
    python3 - "$HIL_SERVER" "$BOARD_MAC" <<'PY'
import json, sys, urllib.request
server, mac = sys.argv[1].rstrip("/"), sys.argv[2].upper().replace("-", ":")
devices = json.load(urllib.request.urlopen(server + "/api/home_intercom/devices", timeout=15))
wanted = False
for key, device in devices.items():
    if str(key).upper().replace("-", ":") == mac:
        wanted = bool(device.get("ota_requested"))
        break
if not wanted:
    print("no leftover ota_requested — skip registry rewrite", flush=True)
    raise SystemExit(0)
print("ota_cancel not on server and ota_requested is set — rewrite registry + restart", flush=True)
raise SystemExit(2)
PY
    local need_restart=$?
    set -e
    if [[ "$need_restart" -eq 0 ]]; then
        return 0
    fi
    echo "=== rewrite device_registry.json and restart $CONTAINER ==="
    hil_ssh "$(docker_path)
set -e
MAC=$(printf '%q' "$BOARD_MAC")
docker exec -u root $CONTAINER python3 -c \"
import json
from pathlib import Path
p = Path('/data/device_registry.json')
data = json.loads(p.read_text())
mac = '$BOARD_MAC'.upper()
devices = data.get('devices') or {}
key = next((k for k in devices if str(k).upper().replace('-', ':') == mac), None)
if key is None:
    raise SystemExit('MAC not in device_registry.json')
devices[key]['ota_requested'] = False
devices[key]['ota_target_version'] = ''
p.write_text(json.dumps(data, indent=2) + chr(10))
print('cleared ota on', key)
\"
docker restart $CONTAINER"
    python3 - "$HIL_SERVER" <<'PY'
import sys, time, urllib.error, urllib.request
url = sys.argv[1].rstrip("/") + "/api/home_intercom/devices"
deadline = time.time() + 60
last = None
while time.time() < deadline:
    try:
        urllib.request.urlopen(url, timeout=5).read()
        print("home-intercom is up", flush=True)
        raise SystemExit(0)
    except Exception as exc:
        last = exc
        time.sleep(1)
print("home-intercom did not come back:", last, file=sys.stderr)
sys.exit(1)
PY
}

do_flash() {
    echo "=== flash ($PHASE) ==="
    ensure_image
    load_meta
    sync_artifacts_to_workspace
    if [[ ! -f "$WS_ARTIFACT_DIR/usb/flash_args" || ! -f "$WS_ARTIFACT_DIR/usb/littlefs.bin" ]]; then
        echo "Incomplete compile artifacts in $ARTIFACT_DIR" >&2
        exit 1
    fi
    hold_hello_pending
    require_hil_mac

    echo "=== flash factory (USB $USB_FW_VERSION) ==="
    # flash_args names bins relative to its directory; that path is the
    # workspace mount inside the container, not the host ARTIFACT_DIR.
    ./docker/dev.sh bash -c \
        "cd /workspace/.hil/usb && python3 $ESPTOOL --chip esp32s3 --port $USB --before default_reset --after hard_reset write_flash @flash_args"
    wait_usb

    echo "=== clear otadata (boot factory slot) ==="
    esptool --chip esp32s3 --port "$USB" \
        --before default_reset --after hard_reset erase_region 0xe000 0x2000
    wait_usb

    echo "=== flash LittleFS ==="
    ./docker/dev.sh bash -c \
        "cd /workspace/.hil/usb && python3 $ESPTOOL --chip esp32s3 --port $USB --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB $LITTLEFS_OFFSET littlefs.bin"
    wait_usb
    sleep 2
}

do_test() {
    echo "=== test ($PHASE) ==="
    ensure_image
    load_meta
    sync_artifacts_to_workspace
    hold_hello_pending
    require_hil_mac

    if [[ "$SKIP_DRY_RUN" != "1" ]]; then
        echo "=== serial confirm_test ==="
        python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB"
    fi

    if [[ "$SKIP_OTA" == "1" ]]; then
        echo "HIL OK (USB + dry-run; LAN OTA skipped)"
        return 0
    fi

    echo "=== approve $BOARD_MAC ==="
    echo "$(manage approve)"

    if [[ ! -f "$ARTIFACT_DIR/ota/firmware.sig" ]]; then
        echo "Missing $ARTIFACT_DIR/ota/firmware.sig — compile with OTA_PRIVATE_KEY or HIL_OTA_KEY" >&2
        exit 1
    fi
    write_ota_json "$OTA_SHA" "$ARTIFACT_DIR/ota/firmware.json"
    if [[ "$SKIP_TAMPER" != "1" ]]; then
        python3 - "$ARTIFACT_DIR/ota/firmware.bad.sig" <<'PY'
from pathlib import Path
import sys
Path(sys.argv[1]).write_bytes(b"\xaa" * 64)
PY
        if [[ "${OTA_SHA:0:1}" == "0" ]]; then
            BAD_SHA="1${OTA_SHA:1}"
        else
            BAD_SHA="0${OTA_SHA:1}"
        fi
        write_ota_json "$BAD_SHA" "$ARTIFACT_DIR/ota/firmware.bad.json"
    fi

    echo "=== stage signed OTA files on NAS ==="
    stage_ota_files

    echo "=== backup firmware cache ==="
    FW_BACKUP="/tmp/hil-fw-backup"
    hil_ssh "$(docker_path)
set -e
rm -rf $FW_BACKUP
mkdir -p $FW_BACKUP
docker cp $CONTAINER:/data/firmware/firmware.bin $FW_BACKUP/firmware.bin
docker cp $CONTAINER:/data/firmware/firmware.json $FW_BACKUP/firmware.json
docker cp $CONTAINER:/data/firmware/firmware.sig $FW_BACKUP/firmware.sig 2>/dev/null || true"

    echo "=== wait hello heartbeat (10s OTA window) ==="
    confirm_test --mode heartbeat

    plant_failed=0
    echo "=== POST ota (GitHub fetch, then plant branch bin) ==="
    OTA_RESP="$(manage ota)" || plant_failed=1
    echo "$OTA_RESP"
    if [[ "$plant_failed" != "0" ]]; then
        echo "manage ota failed" >&2
        manage deapprove >/dev/null || true
        exit 1
    fi

    plant_or_abort() {
        if ! apply_ota_cache "$@"; then
            echo "plant failed — deapprove so the board does not flash GitHub v$OTA_VERSION" >&2
            manage deapprove >/dev/null || true
            exit 1
        fi
    }

    watch_ota() {
        local rc=0
        set +e
        confirm_test "$@"
        rc=$?
        set -e
        if [[ "$rc" -ne 0 ]]; then
            echo "LAN OTA watch failed (rc=$rc)" >&2
            exit "$rc"
        fi
    }

    SEND_OTA=()
    if [[ "$SKIP_TAMPER" != "1" ]]; then
        echo "=== plant wrong X-Checksum-SHA256 ==="
        plant_or_abort firmware.bad.json firmware.sig
        watch_ota --mode ota-fail --expect "SHA-256 mismatch" --timeout 180
        verify_firmware_http "$BAD_SHA" 64

        echo "=== plant wrong firmware.sig ==="
        plant_or_abort firmware.json firmware.bad.sig
        verify_firmware_http "$OTA_SHA" 64
        watch_ota --mode ota-fail --send-ota --expect "ECDSA signature invalid" --timeout 180
        SEND_OTA=(--send-ota)
    fi

    echo "=== plant signed branch firmware ==="
    plant_or_abort firmware.json firmware.sig
    verify_firmware_http "$OTA_SHA" 64

    echo "=== serial watch signed LAN OTA ==="
    if ((${#SEND_OTA[@]})); then
        watch_ota --mode ota-watch --timeout 180 "${SEND_OTA[@]}"
    else
        watch_ota --mode ota-watch --timeout 180
    fi

    if [[ "$SKIP_BUTTONS" != "1" ]]; then
        echo "=== hello buttons map (home-intercom#78) ==="
        wait_usb
        BUTTONS_PLAN="$(hil_buttons_plan)"
        echo "$BUTTONS_PLAN"
        HIL_BUTTONS_RESTORE="$(python3 -c 'import json,sys; print(json.dumps(json.loads(sys.argv[1])["prev"], separators=(",", ":")))' "$BUTTONS_PLAN")"
        BUTTONS_WANT="$(python3 -c 'import json,sys; print(json.loads(sys.argv[1])["want"])' "$BUTTONS_PLAN")"
        BUTTONS_APPLY="$(python3 -c 'import json,sys; print(json.dumps(json.loads(sys.argv[1])["apply"], separators=(",", ":")))' "$BUTTONS_PLAN")"
        echo "$(manage_buttons "$BUTTONS_APPLY")"
        python3 "$ROOT/scripts/hil_confirm_test.py" --port "$USB" --mode buttons \
            --expect "GPIO4 → ${BUTTONS_WANT}" --timeout 35
        restore_hil_buttons
    fi

    echo "HIL OK"
}

case "$PHASE" in
    compile) do_compile ;;
    flash) do_flash ;;
    test) do_test ;;
    all)
        do_compile
        do_flash
        do_test
        ;;
esac
