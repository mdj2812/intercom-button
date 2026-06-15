# Build the dev image:
#   docker build --network host -t esp32-pio-builder:v1.0.0 -f docker/Dockerfile .
#
# Or use dev.sh which auto-builds and launches.
FROM ubuntu:22.04

# ── Use TUNA mirrors (faster in China) ─────────────
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list

# ── System dependencies ─────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv python3-dev \
    git curl wget ca-certificates \
    udev usbutils build-essential \
    && rm -rf /var/lib/apt/lists/*

# ── PlatformIO Core ─────────────────────────────────
RUN pip3 install --no-cache-dir \
    -i https://mirrors.tuna.tsinghua.edu.cn/pypi/web/simple \
    platformio

# ── Pre-install ESP32-S3 toolchain + framework ──────
RUN pio platform install "espressif32"

# ── Workspace ───────────────────────────────────────
# Root user (simplifies volume mount permissions)
WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/root/.platformio

CMD ["/bin/bash"]
