# Build:
#   docker build \
#     --build-context pio-cache=$HOME/.platformio \
#     --network host \
#     -t esp32-pio-builder:v1.0.1 \
#     -f docker/Dockerfile .
#
# Or use dev.sh which handles this automatically.
FROM ubuntu:22.04

# ── Use TUNA mirrors ────────────────────────────────
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list

# ── System dependencies ─────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv python3-dev \
    git curl wget ca-certificates \
    udev usbutils build-essential \
    cppcheck \
    && rm -rf /var/lib/apt/lists/*

# ── Node.js 20.x (required by Gitea Actions runner) ─
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y --no-install-recommends nodejs && \
    rm -rf /var/lib/apt/lists/*

# ── PlatformIO Core ─────────────────────────────────
RUN pip3 install --no-cache-dir \
    -i https://mirrors.tuna.tsinghua.edu.cn/pypi/web/simple \
    platformio

# ── Pre-install ESP32 platform metadata ─────────────
RUN pio platform install "espressif32"

# ── Pre-install native platform (for unit tests) ────
RUN pio platform install "native"

# ── Copy toolchains + framework from host ───────────
# These total ~1.8GB. Host must have them pre-downloaded
# (run `pio run` on the host first).
COPY --from=pio-cache packages/toolchain-xtensa-esp32s3   /root/.platformio/packages/toolchain-xtensa-esp32s3
COPY --from=pio-cache packages/toolchain-riscv32-esp      /root/.platformio/packages/toolchain-riscv32-esp
COPY --from=pio-cache packages/framework-arduinoespressif32 /root/.platformio/packages/framework-arduinoespressif32
COPY --from=pio-cache packages/tool-esptoolpy              /root/.platformio/packages/tool-esptoolpy
COPY --from=pio-cache packages/tool-openocd-esp32           /root/.platformio/packages/tool-openocd-esp32
COPY --from=pio-cache packages/tool-cppcheck               /root/.platformio/packages/tool-cppcheck
COPY --from=pio-cache packages/tool-scons                  /root/.platformio/packages/tool-scons
COPY --from=pio-cache packages/tool-mkspiffs               /root/.platformio/packages/tool-mkspiffs
COPY --from=pio-cache packages/tool-mklittlefs             /root/.platformio/packages/tool-mklittlefs
COPY --from=pio-cache packages/tool-mkfatfs                /root/.platformio/packages/tool-mkfatfs

# ── Verify toolchains ────────────────────────────────
RUN /root/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc --version && \
    /root/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-gcc --version && \
    echo "All toolchains OK"

# ── Workspace ───────────────────────────────────────
WORKDIR /workspace
ENV PLATFORMIO_CORE_DIR=/root/.platformio

CMD ["/bin/bash"]
