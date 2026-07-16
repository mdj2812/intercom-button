#!/usr/bin/env bash
# ────────────────────────────────────────────────────
# ESP32-S3 开发容器 — 一键进入
#
# 用法:
#   ./docker/dev.sh             本地构建或使用已有镜像 → 交互式 shell
#   ./docker/dev.sh pio run     执行命令后退出
#   ./docker/dev.sh -b          强制本地重建
#
# 镜像自包含工具链 (3GB)，无需挂载宿主机 .platformio。
# ────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_FILE="$SCRIPT_DIR/.docker-image"
IMAGE=$(head -1 "$IMAGE_FILE" | tr -d '\n\r')
IMAGE_NAME="${IMAGE##*/}"  # "esp32-pio-builder:v1.0.1"

FORCE_BUILD=false
DO_PUSH=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build) FORCE_BUILD=true; shift ;;
        -p|--push) DO_PUSH=true; shift ;;
        -bp|-pb) FORCE_BUILD=true; DO_PUSH=true; shift ;;
        -D) FORCE_BUILD=true; shift ;;
        *) break ;;
    esac
done

# ── Acquire image ───────────────────────────────────
if ! $FORCE_BUILD; then
    # Try local first, then pull from registry
    if docker image inspect "$IMAGE_NAME" &>/dev/null; then
        docker tag "$IMAGE_NAME" "$IMAGE" 2>/dev/null || true
    elif docker image inspect "$IMAGE" &>/dev/null; then
        :   # already have it
    elif docker pull "$IMAGE"; then
        docker tag "$IMAGE" "$IMAGE_NAME"
        echo "=== Pulled $IMAGE ==="
    else
        echo "=== Image not found: $IMAGE ===" >&2
        echo "Use -b to build locally, or wait for CI to publish to ghcr.io." >&2
        exit 1
    fi
fi

if $FORCE_BUILD; then
    echo "=== Building $IMAGE (~3GB) ==="
    docker build \
        --build-context "pio-cache=$HOME/.platformio" \
        --network host \
        -t "$IMAGE" \
        -t "$IMAGE_NAME" \
        -f "$SCRIPT_DIR/Dockerfile" \
        "$PROJECT_ROOT"
    echo "=== Build complete ==="

    if $DO_PUSH; then
        echo "=== Pushing $IMAGE ==="
        docker push "$IMAGE"
        echo "=== Pushed ==="
    fi
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
if [ $# -eq 0 ]; then
    exec docker run --rm -it \
        --network host \
        $DEVICE_FLAG \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        /bin/bash
else
    exec docker run --rm \
        --network host \
        $DEVICE_FLAG \
        -v "$PROJECT_ROOT:/workspace" \
        "$IMAGE" \
        "$@"
fi
