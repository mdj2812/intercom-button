#!/usr/bin/env bash
# ────────────────────────────────────────────────────
# ESP32-S3 Intercom Button — 一键开发容器
#
# 用法:
#   ./docker/dev.sh             交互式 shell
#   ./docker/dev.sh pio run     执行命令后退出
#   ./docker/dev.sh -b          强制重建镜像
#
# Mounts host ~/.platformio to avoid re-downloading
# toolchains + framework via PlatformIO's unreliable CDN.
# ────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_FILE="$SCRIPT_DIR/.docker-image"
IMAGE=$(head -1 "$IMAGE_FILE" | tr -d '\n\r')

FORCE_BUILD=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build) FORCE_BUILD=true; shift ;;
        -D) FORCE_BUILD=true; shift ;;
        *) break ;;
    esac
done

# ── Build image if needed ───────────────────────────
if $FORCE_BUILD || ! docker image inspect "$IMAGE" &>/dev/null; then
    echo "=== Building $IMAGE ==="
    docker build --network host -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$PROJECT_ROOT"
    echo "=== Build complete ==="
fi

# ── Detect USB device ───────────────────────────────
USB_DEV=""
for dev in /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$dev" ] && USB_DEV="$dev" && break
done
DEVICE_FLAG=""
[ -n "$USB_DEV" ] && DEVICE_FLAG="--device=$USB_DEV" && echo "USB: $USB_DEV"
[ -z "$USB_DEV" ] && echo "⚠️  No USB device found — upload won't work"

# ── Run ─────────────────────────────────────────────
# Mount host PlatformIO cache: toolchains, framework, libs
# This avoids re-downloading ~1GB via PlatformIO's flaky CDN.
if [ $# -eq 0 ]; then
    exec docker run --rm -it \
        --network host \
        $DEVICE_FLAG \
        -v "$HOME/.platformio:/root/.platformio" \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        /bin/bash
else
    exec docker run --rm \
        --network host \
        $DEVICE_FLAG \
        -v "$HOME/.platformio:/root/.platformio" \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        "$@"
fi
