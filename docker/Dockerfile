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
# Pin Core so image rebuilds are reproducible. Bump this when upgrading.
RUN pip3 install --no-cache-dir platformio==6.2.0

# ── Pre-install ESP32 platform metadata ─────────────
RUN pio platform install "espressif32"

# ── Pre-install native platform (for unit tests) ────
RUN pio platform install "native"

# ── Copy toolchains + framework from host ───────────
# Copies whatever packages pio run downloaded — GitHub Actions
# runner pre-downloads them in the workflow, local dev via pio run.
COPY --from=pio-cache packages/ /root/.platformio/packages/

WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/root/.platformio

# Filesystem image tools used by `pio run -t upload` / uploadfs.
# `pio run` (compile only) does not pull these, so bake them in.
RUN pio pkg install --global --silent \
      -t "platformio/tool-mkspiffs@~2.230.0" \
      -t "platformio/tool-mklittlefs@~1.203.0" \
      -t "platformio/tool-mkfatfs@~2.0.0"

# ── Verify toolchains ────────────────────────────────
RUN /root/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc --version && \
    echo "Toolchain OK" && \
    pio pkg list --global --only-tools

CMD ["/bin/bash"]
