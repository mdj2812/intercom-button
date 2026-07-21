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
    "server_host": "192.168.99.4",
    "server_port": 8123,
    "ha_token": "",
    "sample_rate": 16000,
    "max_record_secs": 60,
    "buttons": {
        "4": "study",
        "5": "living",
        "12": "cinema",
        "13": "bedroom"
    }
}
```

| Field | Description |
|-------|-------------|
| `server_scheme` | `http` for a trusted LAN; `https` for remote HA. HTTPS traffic is encrypted, but this firmware currently does not verify the server certificate. |
| `server_host` | Home Assistant IP (or Docker host for legacy mode) |
| `server_port` | `8123` for HA integration, `8764` for legacy Docker |
| `ha_token` | HA Long-Lived Access Token. **Leave empty** for Docker mode. For HA mode, [create a restricted token](https://www.home-assistant.io/docs/authentication/#your-account-profile) with minimal permissions — never use your admin token. The token is stored in plaintext on the ESP32 flash; if the device is physically compromised, revoke it from the HA UI. |
| `buttons` | Per-GPIO-pin room target defaults. Keys are GPIO numbers, values are room IDs from `rooms.json` (`study`, `living`, `cinema`, `bedroom`, or `all` for broadcast). |
| `sample_rate` | Audio sample rate in Hz (default: 16000) |
| `max_record_secs` | Maximum recording duration in seconds (default: 60) |

Copy `data/config.example.json` to `data/config.json` and fill in your settings. `data/config.json` is gitignored — credentials stay local.

When `ha_token` is set, the `Authorization: *** header is added. Without a token, no auth header is sent (Docker-compatible).

**Multi-button setup**: flash firmware once, then for each device edit `data/config.json` (change `buttons` mapping) and run `pio run -e esp32-s3-devkitc-1 -t uploadfs`.

## Quick Start

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

1. Connect ESP32-S3 via USB-C
2. `make flash`
3. `make flashfs`   (after editing `data/config.json`)

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
| 🟢 Green | Ready (WiFi connected, idle) |
| 🔴 Red blinking | WiFi disconnected |
| 🔵 Blue | Recording |
| ⚪ White blinking | Uploading |
| 🟢 Flash ×4 | Upload success |
| 🔴 Flash ×4 | Upload failed |

### Recording behavior

- **Hold to talk**: press button → record, release → send
- **Minimum recording**: 500ms (shorter taps are discarded)
- **Maximum recording**: configured in `data/config.json` (`max_record_secs`, default 60 seconds)
- **Format**: 16-bit PCM WAV, 16kHz mono

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
│   ├── test_config/         # Config parsing tests
│   ├── test_config_manager/ # Config manager tests
│   ├── test_http_uploader/  # HTTP upload tests
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
    ├── http_uploader.h/cpp  # POST /record?target=<room>
    ├── button_manager.h/cpp # Multi-button GPIO matrix + debounce
    ├── room_target_store.h/cpp # NVS room target storage
    └── ota_manager.h/cpp    # OTA firmware update (optional)
```

## Environments

This project has **two PlatformIO environments** — always specify which one to use:

| Environment | Platform | Purpose |
|-------------|----------|---------|
| `esp32-s3-devkitc-1` | xtensa-esp32s3 | Firmware — compile, flash, monitor |
| `native` | x86_64 Linux | Unit tests — run on host, no ESP32 needed |

Without `-e`, `pio run` builds **both** environments. The native env has `src_filter = -<src/*>` so it only compiles test files — never Arduino-dependent source code.

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
```

CI enforces formatting — PRs with style violations will fail the `format` job.

## Debugging

### Serial monitor

```bash
make monitor
```

The firmware logs every state transition:

```
=== ESP32-S3 Intercom Button ===
[cfg] Loaded: server=https://ha.example.com:443 wifi=MyWiFi buttons=4
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
| Upload OK but no sound | Wrong room key | Verify `room` in `data/config.json` matches a key in `rooms.json` (`study`/`living`/`cinema`/`bedroom`) |
| Config not loading | LittleFS not flashed | Run `make flashfs` to upload the file system |
| PSRAM allocation warning | Board variant mismatch | Verify `board_build.psram_type = opi` in `platformio.ini` |

### Inspect build details

```bash
# Show memory usage
make size

# Clean rebuild
make clean && make
```

## License

MIT
