# Build the dev image:
#   docker build --network host -t esp32-pio-builder:v1.0.0 -f docker/Dockerfile .
#
# Or use dev.sh which auto-builds and launches.
FROM ubuntu:22.04

# ── Use TUNA mirrors ────────────────────────────────
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

# ── Pre-install ESP32 platform ──────────────────────
# Downloads platform metadata + generic ESP32 toolchains.
RUN pio platform install "espressif32"

# ── Pre-warm: download board-specific toolchains ────
# `pio platform install` only installs generic ESP32 packages.
# Our project needs S3-specific toolchains (xtensa-esp32s3, riscv32-esp).
# Copy project files and run `pio run` once. Compilation will fail
# (placeholder WiFi creds), but toolchains + framework are cached.
COPY platformio.ini /tmp/warm/
COPY src/ /tmp/warm/src/
RUN cd /tmp/warm && (pio run 2>&1 || true)
RUN rm -rf /tmp/warm

# ── Workspace ───────────────────────────────────────
WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/root/.platformio

CMD ["/bin/bash"]
