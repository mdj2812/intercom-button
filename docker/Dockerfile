# Build:
#   docker build \
#     --build-context pio-cache=$HOME/.platformio \
#     --network host \
#     -t esp32-pio-builder:v1.0.1 \
#     -f docker/Dockerfile .
#
# Or use dev.sh which handles this automatically.
FROM ubuntu:22.04

# ── System dependencies ─────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv python3-dev \
    git curl wget ca-certificates \
    udev usbutils build-essential \
    cppcheck \
    lcov \
    && rm -rf /var/lib/apt/lists/*

# ── Node.js 20.x (required by Gitea Actions runner) ─
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y --no-install-recommends nodejs && \
    rm -rf /var/lib/apt/lists/*

# ── PlatformIO Core ─────────────────────────────────
RUN pip3 install --no-cache-dir platformio

# ── Pre-install ESP32 platform metadata ─────────────
RUN pio platform install "espressif32"

# ── Pre-install native platform (for unit tests) ────
RUN pio platform install "native"

# ── Copy toolchains + framework from host ───────────
# Copies whatever packages pio run downloaded — GitHub Actions
# runner pre-downloads them in the workflow, local dev via pio run.
# Single glob copy avoids failing on missing optional packages
# (mkspiffs, openocd, etc.) that pio run doesn't always pull.
COPY --from=pio-cache packages/ /root/.platformio/packages/

# ── Verify toolchains ────────────────────────────────
RUN /root/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc --version && \
    echo "Toolchain OK"

# ── Workspace ───────────────────────────────────────
WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/root/.platformio

CMD ["/bin/bash"]
