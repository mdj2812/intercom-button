# Build the dev image:
#   docker build -t esp32-pio-builder:v1.0.0 -f docker/Dockerfile .
#
# Or use dev.sh which auto-builds and launches.
FROM ubuntu:22.04

# ── System dependencies ─────────────────────────────
# python3-dev + build-essential for PlatformIO native extensions
# udev + usbutils for USB device access (flashing)
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv python3-dev \
    git curl wget ca-certificates \
    udev usbutils build-essential \
    && rm -rf /var/lib/apt/lists/*

# ── PlatformIO Core ─────────────────────────────────
# Install via pip (faster than installer script)
RUN pip3 install --no-cache-dir platformio

# ── Pre-install ESP32-S3 toolchain + framework ──────
# This downloads ~500MB of toolchains (xtensa-esp32s3, riscv32-esp),
# the Arduino-ESP32 framework, and the platform itself.
# Without this, first `pio run` takes 5-10 minutes.
RUN pio platform install "espressif32"

# ── Dev user ────────────────────────────────────────
# Non-root user for safety; dialout group for USB serial access
RUN useradd -m -s /bin/bash dev && \
    echo "dev ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
    usermod -a -G dialout dev

USER dev
WORKDIR /workspace

# ── PlatformIO cache in home directory ──────────────
# Already populated by the pio platform install above
ENV PLATFORMIO_CORE_DIR=/home/dev/.platformio

CMD ["/bin/bash"]
