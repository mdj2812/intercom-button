#!/usr/bin/env bash
# ────────────────────────────────────────────────────
# ESP32-S3 Intercom Button — 一键开发容器
#
# 用法:
#   ./docker/dev.sh             交互式 shell
#   ./docker/dev.sh pio run     执行命令后退出
#   ./docker/dev.sh -b          强制重建镜像
#
# 前提: Docker 已安装，USB 线连接 ESP32-S3
# ────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_FILE="$SCRIPT_DIR/.docker-image"
IMAGE=$(cat "$IMAGE_FILE" | head -1 | tr -d '\n\r')
IMAGE_NAME="${IMAGE%%:*}"
IMAGE_TAG="${IMAGE##*:}"

FORCE_BUILD=false
EXTRA_ARGS=()

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build) FORCE_BUILD=true; shift ;;
        -D) FORCE_BUILD=true; EXTRA_ARGS+=("$1"); shift ;;
        *) break ;;
    esac
done

# ── Build image if needed ───────────────────────────
if $FORCE_BUILD || ! docker image inspect "$IMAGE" &>/dev/null; then
    echo "=== Building $IMAGE ==="
    docker build \
        --network host \
        -t "$IMAGE" \
        -f "$SCRIPT_DIR/Dockerfile" \
        "$PROJECT_ROOT"
    echo "=== Build complete ==="
fi

# ── Detect USB device for ESP32-S3 ─────────────────
# CP210x or CH343 USB-UART bridge
USB_DEVICE=""
for dev in /dev/ttyUSB* /dev/ttyACM*; do
    if [ -e "$dev" ]; then
        USB_DEVICE="$dev"
        break
    fi
done

DOCKER_DEVICE_FLAG=""
if [ -n "$USB_DEVICE" ]; then
    echo "Found USB device: $USB_DEVICE"
    DOCKER_DEVICE_FLAG="--device=$USB_DEVICE"
else
    echo "⚠️  No /dev/ttyUSB* or /dev/ttyACM* found — upload won't work"
fi

# ── Run container ───────────────────────────────────
if [ $# -eq 0 ]; then
    # Interactive mode
    exec docker run --rm -it \
        --network host \
        $DOCKER_DEVICE_FLAG \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        /bin/bash
else
    # Command mode
    exec docker run --rm \
        --network host \
        $DOCKER_DEVICE_FLAG \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        "$@"
fi
