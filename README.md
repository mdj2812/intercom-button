# Intercom Button

ESP32-S3 push-to-talk desktop button for the [home-intercom](https://github.com/mdj2812/home-intercom) system. Press and hold to record, release to broadcast to a target room via Xiaomi smart speakers.

## Demo

![Demo](docs/demo.gif)

*Press & hold → record voice → release → broadcast to the study room speaker. [Full quality video](docs/demo.mp4)*

## Hardware

| Component | Notes |
|-----------|-------|
| **MCU** | ESP32-S3-DevKitC (WROOM-1 N8) |
| **Microphone** | MAX9814 electret mic module with AGC (¥5-6) |
| **Button** | Any momentary push button (¥2) |
| **BOM total** | ~¥8 (MCU already owned) |

### Wiring

| ESP32-S3 | MAX9814 | Button |
|----------|---------|--------|
| GPIO1 (ADC1_CH0) | OUT | — |
| 3.3V | VCC | — |
| GND | GND | one leg |
| GPIO4 | — | other leg → GND |

MAX9814 gain: solder GAIN pad to GND for 50dB (recommended for desktop use).

### Per-device config

Configuration is stored in LittleFS (`data/config.json`), **not** in the source code.
The same firmware binary works for all rooms — just upload a different config file.

```json
{
    "wifi_ssid": "your_wifi",
    "wifi_password": "your_password",
    "server_scheme": "http",
    "server_host": "homeassistant.local",
    "server_port": 8123,
    "pins": [4, 5, 12, 13]
}
```

| Field | Description |
|-------|-------------|
| `server_scheme` | `http` for a trusted LAN; `https` for remote HA. HTTPS traffic is encrypted, but this firmware currently does not verify the server certificate. |
| `server_host` | Home Assistant IP (or Docker host for legacy mode) |
| `server_port` | `8123` for HA integration, `8764` for legacy Docker |
| `pins` | Hardware GPIOs to initialize. Omit to use compile-time `{4,5,12,13}`. Room targets come from hello `buttons`, not this file. |

Copy `data/config.example.json` to `data/config.json` and fill in your settings. `data/config.json` is gitignored — WiFi credentials stay local.

The device identifies itself with its Wi-Fi MAC (`X-Device-ID`). After WiFi connects it calls `GET /api/home_intercom/config` for `sample_rate` / `max_record_secs` (compile-time fallback 16000 Hz / 60 s if the server is down), then `POST /api/home_intercom/devices/hello` (trust-on-first-use). The hello `buttons` object is the GPIO→room map (home-intercom#78); empty `{}` means unconfigured and last NVS is kept. Hello repeats every 10 seconds while idle so Home Assistant `last_seen` / Online stay current and a PWA map edit is applied without reboot. Uploads go to `/api/home_intercom/device/record`. No Home Assistant token is stored on the ESP32. Unknown or revoked MACs receive HTTP 403; revoke a lost device from the HA UI. A heartbeat that sees revoked/pending drops back to the orange wait.

**Multi-button setup**: flash firmware once. Which GPIO is which button stays in `pins`. Bind each GPIO to a room in the Home Intercom PWA. Re-upload LittleFS with `pio run -e esp32-s3-devkitc-1 -t uploadfs` after changing pins.

## Quick Start

First board: USB `make flash` then `make flashfs`. Later upgrades: leave the GitHub `.bin` and `.sig` on the release; Home Intercom **Update** does LAN OTA. The `.sig` is verified on the device, not used to sign. Details under [GitHub release](#github-release) and [LAN OTA](#lan-ota).

### With Make (recommended)

```bash
make            # compile firmware
make flash      # compile + flash via USB
make flashfs    # upload LittleFS config (data/config.json)
make monitor    # serial monitor
make test       # run unit tests (no hardware needed)
make check      # static analysis
make clean      # clean build artifacts
make format     # auto-format code
make size       # show firmware memory usage
```

### Docker

```bash
# Pull image + enter interactive shell
./docker/dev.sh

# One-shot commands via Docker
make docker-build     # compile in container
make docker-flash     # flash in container (auto-mounts USB)
make docker-flashfs   # upload LittleFS config
make docker-test      # unit tests in container
make docker-shell     # interactive shell in container
```

### Bare metal (no Docker)

Same `make` commands work — just requires [PlatformIO Core](https://platformio.org/install/cli):

```bash
pip install platformio
make flash
```

## Commands

| Command | What it does |
|---------|-------------|
| `make` | Compile firmware |
| `make flash` | Compile + flash via USB |
| `make flashfs` | Upload LittleFS config (`data/config.json`) |
| `make monitor` | Open serial monitor |
| `make test` | Run unit tests on host (no ESP32 needed) |
| `make check` | Static analysis (cppcheck) |
| `make clean` | Clean build artifacts |
| `make format` | Auto-format code with clang-format |
| `make format-check` | Check formatting (CI) |
| `make size` | Show firmware memory usage |

Docker variants: prefix with `docker-` (e.g. `make docker-build`, `make docker-flash`).

Raw PlatformIO commands (if you prefer):

| Command | What it does |
|---------|-------------|
| `pio run -e esp32-s3-devkitc-1` | Compile firmware |
| `pio run -e esp32-s3-devkitc-1 -t upload` | Compile + flash |
| `pio run -e esp32-s3-devkitc-1 -t uploadfs` | Upload LittleFS config |
| `pio run -e esp32-s3-devkitc-1 -t clean` | Clean build artifacts |
| `pio test -e native` | Run unit tests |
| `pio check -e esp32-s3-devkitc-1` | Static analysis |

### Flash via USB (first time)

USB is first install and recovery. It writes the bootloader, this project's 8MB OTA partition table, and the factory app. The release `.sig` is **not** used on USB.

1. Connect the ESP32-S3 over USB
2. `make flash` (or `make docker-flash`)
3. `make flashfs` after editing `data/config.json`

A blank board should be flashed from the git tag that matches the firmware version you want, so bootloader, partitions, and app stay together. LittleFS (`config.json`) is never in the GitHub release.

### GitHub release

Each [GitHub Release](https://github.com/mdj2812/intercom-button/releases) publishes:

| Asset | What it is |
|-------|------------|
| `intercom-button-vX.Y.Z.bin` | Application image (factory slot at `0x10000`) |
| `intercom-button-vX.Y.Z.bin.sig` | 64-byte ECDSA secp256r1 signature of the SHA-256 of that `.bin` (raw r and s, 32 bytes each) |

CI signs the `.bin` with repo secret `OTA_PRIVATE_KEY`. The matching **public** key is compiled into the firmware (`src/ota_keys.h`). You never need the private key. The board does **not** sign firmware; OTA only **verifies** a signature that already exists.

**USB from the release `.bin`:** flash the `.bin` only — do not write the `.sig` to flash. This overwrites the factory app at `0x10000` and assumes the chip already has this repo's partition table (after a previous `make flash`).

```bash
esptool.py --chip esp32s3 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB \
  0x10000 intercom-button-vX.Y.Z.bin
```

Then upload your own LittleFS with `make flashfs`.

### LAN OTA

The ESP32 OTA client is **HTTP-only**. It cannot download GitHub assets (HTTPS). [home-intercom](https://github.com/mdj2812/home-intercom) fetches the latest GitHub `.bin` and `.sig`, caches them, and serves:

- `GET /api/home_intercom/firmware` with `X-Checksum-SHA256`
- `GET /api/home_intercom/firmware.sig`

In the Home Intercom PWA, **Update** on a device caches that image and sets the next hello to `"ota": true`. The idle button then:

1. Downloads `.sig` (exactly 64 bytes, or skips ECDSA if it is missing)
2. Downloads `.bin` and hashes it
3. Requires the SHA-256 header to match when present
4. If a `.sig` was present, verifies ECDSA against the **currently running** public key
5. Writes the inactive OTA slot and reboots (LED is solid orange while flashing)

A bad checksum or bad `.sig` aborts; the running image stays. A missing `.sig` still flashes using SHA-256 only (or with a warning if the header is also absent).

After reboot, a parsed `/devices/hello` (`ok` / `pending` / `revoked`) within 60 seconds marks the image valid. A button press or serial `confirm` is a shortcut. No confirm in 60s rolls back. Serial `ota` while idle also starts a download from the same LAN URLs.

**Public-key rotation:** if `ota_keys.h` changed since the board last updated, a GitHub `.sig` will fail ECDSA until that board takes the new image once over USB (or unsigned OTA).

### Docker-specific

```bash
# Interactive dev shell
./docker/dev.sh

# Convenience make targets (auto-handles image + USB)
make docker-build
make docker-flash
make docker-test
make docker-shell

# Rebuild Docker image locally
./docker/dev.sh -b
```

## Architecture

```
┌──────────────┐   16kHz ADC     ┌───────────────┐   WAV POST   ┌──────────────┐
│  MAX9814 Mic │ ──────────────→ │  ESP32-S3     │ ───────────→ │ Flask Server │
│  (analog)    │   GPIO1 (ADC)   │  PSRAM buffer  │  /convert    │  :8764       │
└──────────────┘                 │  WiFi STA      │              └──────┬───────┘
                                 │  WS2812 LED    │                     │
┌──────────────┐                 │  Push button   │              ┌──────▼───────┐
│   Button     │ ──────────────→ │  (active low)  │              │  Home        │
│   (GPIO4)    │   INT + PU      └───────────────┘              │  Assistant   │
└──────────────┘                                                 │  play_media  │
                                                                 └──────────────┘
```

### LED Status

| Color | Meaning |
|-------|---------|
| 🟢 Green | Ready (WiFi connected, registered, idle) |
| 🔴 Red blinking | WiFi disconnected |
| 🟠 Orange blinking | Registering with the server (`/devices/hello`) |
| 🟠 Orange solid | LAN OTA download / flash in progress |
| 🔵 Blue | Recording |
| ⚪ White blinking | Uploading |
| 🟢 Flash ×4 | Upload success |
| 🔴 Flash ×4 | Upload failed |

### Recording behavior

- **Hold to talk**: press button → record, release → send
- **Minimum recording**: 500ms (shorter taps are discarded)
- **Maximum recording**: from `GET /api/home_intercom/config` (and hello); compile-time fallback 60 seconds
- **Format**: 16-bit PCM WAV, 16kHz mono (server `sample_rate`, default 16000)

## Project Structure

```text
intercom-button/
├── .clang-format            # C++ code style rules
├── .editorconfig            # Editor settings
├── Makefile                 # Convenience commands
├── platformio.ini           # PlatformIO project config
├── .github/
│   └── workflows/ci.yml     # CI pipeline (build, check, test, format, coverage)
├── docker/
│   ├── Dockerfile           # Self-contained dev image
│   ├── .docker-image        # Image version tag
│   └── dev.sh               # One-command dev container
├── data/
│   ├── config.example.json  # Template (committed)
│   └── config.json          # Your settings (gitignored, uploaded to LittleFS)
├── test/
│   ├── mocks/               # Mock Arduino/ESP headers
│   ├── test_audio_recorder/ # Timer ISR / ADC / PSRAM audio tests
│   ├── test_firmware_main/  # setup()/loop() state machine (native mocks)
│   ├── test_config/         # Config parsing tests
│   ├── test_config_manager/ # Config manager tests
│   ├── test_http_uploader/  # HTTP upload tests
│   ├── test_device_id/      # MAC identity tests
│   ├── test_device_hello/   # /devices/hello registration tests
│   ├── test_server_config/  # GET /config audio settings tests
│   ├── test_wifi_manager/   # WiFi manager tests
│   ├── test_button_manager/ # Button manager tests
│   └── test_room_target_store/ # Room store tests
└── src/
    ├── main.cpp             # State machine: IDLE→RECORDING→UPLOADING
    ├── config.h             # Pin definitions
    ├── consts.hpp           # Shared constants
    ├── config_manager.h/cpp # JSON config loader (LittleFS)
    ├── wifi_manager.h/cpp   # Non-blocking WiFi + auto-reconnect
    ├── audio_recorder.h/cpp # 16kHz timer ISR → PSRAM → WAV
    ├── http_uploader.h/cpp  # POST /device/record?target=<room>
    ├── device_id.h/cpp      # STA MAC → X-Device-ID
    ├── device_hello.h/cpp   # POST /devices/hello registration
    ├── server_config.h/cpp  # GET /config audio settings
    ├── button_manager.h/cpp # Multi-button GPIO matrix + debounce
    ├── room_target_store.h/cpp # NVS room target storage
    ├── ota_keys.h           # Compiled-in ECDSA public key (verify only)
    └── ota_manager.h/cpp    # LAN OTA download, SHA-256 + ECDSA verify
```

## Environments

This project has **two PlatformIO environments** — always specify which one to use:

| Environment | Platform | Purpose |
|-------------|----------|---------|
| `esp32-s3-devkitc-1` | xtensa-esp32s3 | Firmware — compile, flash, monitor |
| `native` | x86_64 Linux | Unit tests — run on host, no ESP32 needed |

Without `-e`, `pio run` builds **both** environments. Native tests compile `src/` except `main.cpp` (that file is pulled into `test_firmware_main` so Unity can call `setup()`/`loop()`).

```bash
# ✅ Correct
make                    # or: pio run -e esp32-s3-devkitc-1
make test               # or: pio test -e native

# ❌ Wrong — tries to build native too, causing Arduino.h errors
pio run                 # (unless you specify -e)
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Adafruit NeoPixel | ^1.12.0 | WS2812 RGB LED control |
| ArduinoJson | ^6.21.0 | JSON config parsing |
| Arduino-ESP32 | ~3.20017.0 | HAL, WiFi, HTTPClient, LittleFS |
| ESP32-S3 toolchain | 8.4.0+2021r2 | Xtensa + RISC-V compilers |

All managed by PlatformIO — no manual installation needed.

## Code Style

C++ code follows [`.clang-format`](.clang-format) (LLVM-based, 4-space indent, 120-char limit). Editor settings in [`.editorconfig`](.editorconfig).

```bash
make format        # auto-format all source files
make format-check  # check formatting without modifying (CI)
make compiledb     # compile_commands.json for clangd (uses the dev Docker image)
```

Clangd on the host cannot see `/root/.platformio` inside Docker. `make compiledb` copies the firmware packages into `.clangd-pio`, generates `compile_commands.json`, and rewrites paths. Install the **clangd** editor extension (`llvm-vs-code-extensions.vscode-clangd`), disable Microsoft C/C++ IntelliSense, then reload the window. Re-run `make compiledb` after `platformio.ini` or image changes.

CI enforces formatting — PRs with style violations will fail the `format` job.

## Debugging

### Serial monitor

```bash
make monitor
```

The firmware logs every state transition:

```
=== ESP32-S3 Intercom Button ===
[cfg] Loaded: server=https://ha.example.com:443 wifi=MyWiFi pins=4
[wifi] Connecting to MyWiFi...
[wifi] Connected
[audio] Buffer: 960000 samples (60 sec), PSRAM free: 7654 KB
[audio] Ready — ISR @ 16000 Hz
[main] Setup complete — ready.
[main] Recording...
[audio] Stopped — 48000 samples (3.0s)
[upload] POST https://ha.example.com:443/api/home_intercom/device/record?target=study (96044 bytes) attempt 1/3
[upload] OK: {"ok":true,"rooms_sent":1,...}
[main] Upload OK (1725 ms)
```

### Common issues

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Red LED blinking | WiFi can't connect | Check SSID/password in `data/config.json`, re-upload with `make flashfs` |
| Upload fails | Wrong USB port | `pio device list`, then `make flash` (auto-detects) |
| Recording but no upload | Server unreachable | Check `server_host` in `data/config.json` |
| Upload timeout (ESP32 says failed but audio played) | HA response too slow | Known benign issue — audio was delivered, retry logic handles it |
| Upload OK but no sound | Wrong room key | Confirm the PWA GPIO→room map (hello `buttons`) matches speakers. Unassigned GPIOs skip upload. Empty hello `{}` keeps last NVS. |
| Config not loading | LittleFS not flashed | Run `make flashfs` to upload the file system |
| PSRAM allocation warning | Board variant mismatch | Verify `board_build.psram_type = opi` in `platformio.ini` |
| `[ota] FAILED: ECDSA signature invalid` | Running image has a different public key, or `.sig` does not match the `.bin` | USB-flash the matching tag once (or OTA without a `.sig`); do not copy `.sig` onto the chip |
| `[ota] SHA-256 mismatch` | Cached firmware vs `X-Checksum-SHA256` disagree | Re-run PWA **Update** so home-intercom recaches GitHub `.bin` / `.sig` |
| New image rolls back after ~60s | Hello did not confirm the boot | Leave the panel on the LAN so hello succeeds, or serial `confirm` / press a button |
| PWA Update does nothing on the button | GitHub assets are HTTPS; the ESP32 never fetches them itself | Confirm home-intercom cached the release and the next hello has `"ota": true` |

### Inspect build details

```bash
# Show memory usage
make size

# Clean rebuild
make clean && make
```

## License

MIT
