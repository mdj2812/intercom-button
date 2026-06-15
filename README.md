# ESP32-S3 Desktop Intercom Button

Push-to-talk desktop button for the [home-intercom](https://gitea.home.mdj2812.top/home_lab/home-intercom) system. Press and hold to record, release to broadcast to a target room via Xiaomi smart speakers.

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
    "server_host": "192.168.99.10",
    "server_port": 8764,
    "room": "study",
    "sample_rate": 16000,
    "max_record_secs": 60
}
```

Room keys (from `rooms.json`): `study`, `living`, `cinema`, `bedroom`, or `all` for broadcast.

Copy `data/config.example.json` to `data/config.json` and fill in your settings. `data/config.json` is gitignored — credentials stay local.

**Multi-button setup**: flash firmware once, then for each device edit `data/config.json` (change `room`) and run `pio run -e esp32-s3-devkitc-1 -t uploadfs`.

## Quick Start

### Docker (recommended)

```bash
# Pull image from local registry + enter dev shell
./docker/dev.sh

# Or build + push a new version
./docker/dev.sh -b -p
```

Then inside the container:

```bash
# First time: flash firmware + configuration
pio run -e esp32-s3-devkitc-1 -t upload        # flash firmware
# Edit data/config.json with your WiFi & room
pio run -e esp32-s3-devkitc-1 -t uploadfs      # flash LittleFS config

# Unit tests (no hardware needed)
pio test -e native

# Static analysis
pio check -e esp32-s3-devkitc-1

# Recompile after changes
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1 -t upload
pio device monitor
```

### Bare metal (no Docker)

Requires [PlatformIO Core](https://platformio.org/install/cli):

```bash
pip install platformio

# Compile & flash firmware
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1 -t upload
pio run -e esp32-s3-devkitc-1 -t uploadfs   # flash LittleFS config

# Unit tests (no hardware needed)
pio test -e native

# Static analysis
pio check -e esp32-s3-devkitc-1
```

## Commands

| Command | What it does |
|---------|-------------|
| `pio run -e esp32-s3-devkitc-1` | Compile firmware for ESP32-S3 |
| `pio run -e esp32-s3-devkitc-1 -t upload` | Compile + flash via USB |
| `pio run -e esp32-s3-devkitc-1 -t uploadfs` | Upload LittleFS config (`data/config.json`) |
| `pio run -e esp32-s3-devkitc-1 -t clean` | Clean build artifacts |
| `pio test -e native` | Run unit tests on host (no ESP32 needed) |
| `pio check -e esp32-s3-devkitc-1` | Static analysis (cppcheck) |
| `pio device monitor` | Open serial monitor (Ctrl+C to exit) |
| `pio device monitor -b 115200` | Serial monitor with explicit baud rate |
| `pio device list` | List connected serial devices |
| `pio run -v -e esp32-s3-devkitc-1` | Verbose build (see exact compiler flags) |

### Flash via USB (first time)

1. Connect ESP32-S3 via USB-C
2. Check device: `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`
3. `pio run -e esp32-s3-devkitc-1 -t upload`
4. PlatformIO auto-detects the port and flashes

If auto-detect fails, specify the port:

```bash
pio run -e esp32-s3-devkitc-1 -t upload --upload-port /dev/ttyUSB0
```

### Docker-specific

```bash
# Interactive dev shell
./docker/dev.sh

# Run a single command
./docker/dev.sh pio run -e esp32-s3-devkitc-1
./docker/dev.sh pio run -e esp32-s3-devkitc-1 -t upload
./docker/dev.sh pio test -e native
./docker/dev.sh pio check -e esp32-s3-devkitc-1

# Rebuild Docker image locally
./docker/dev.sh -b

# Build + push to local registry
./docker/dev.sh -b -p
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

```
esp32-intercom-button/
├── platformio.ini          # PlatformIO project config
├── docker/
│   ├── Dockerfile           # Self-contained dev image (3GB)
│   ├── .docker-image        # Full registry path (source of truth)
│   └── dev.sh               # One-command dev container
├── data/
│   ├── config.example.json  # Template (committed)
│   └── config.json          # Your settings (gitignored, uploaded to LittleFS)
└── src/
    ├── main.cpp             # State machine: IDLE→RECORD→UPLOAD
    ├── config_manager.h/cpp # JSON config loader (LittleFS)
    ├── wifi_manager.h/cpp   # Non-blocking WiFi + auto-reconnect
    ├── audio_recorder.h/cpp # 16kHz timer ISR → PSRAM → WAV
    └── http_uploader.h/cpp  # POST /convert?target=<room>
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
pio run -e esp32-s3-devkitc-1
pio test -e native

# ❌ Wrong — tries to build native too, causing Arduino.h errors
pio run
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Adafruit NeoPixel | ^1.12.0 | WS2812 RGB LED control |
| ArduinoJson | ^6.21.0 | JSON config parsing |
| Arduino-ESP32 | ~3.20017.0 | HAL, WiFi, HTTPClient, LittleFS |
| ESP32-S3 toolchain | 8.4.0+2021r2 | Xtensa + RISC-V compilers |

All managed by PlatformIO — no manual installation needed.

## Debugging

### Serial monitor

```bash
pio device monitor
```

The firmware logs every state transition:

```
=== ESP32-S3 Intercom Button ===
[cfg] Loaded: room=study server=192.168.99.10:8764 wifi=MyWiFi
[wifi] Connecting to MyWiFi...
[wifi] Connected
[audio] Buffer: 960000 samples (60 sec), PSRAM free: 7654 KB
[audio] Ready — ISR @ 16000 Hz
[main] Setup complete — ready.
[main] Recording...
[audio] Stopped — 48000 samples (3.0s)
[upload] POST http://192.168.99.10:8764/convert?target=study (96044 bytes) attempt 1/3
[upload] OK: {"ok":true,"rooms_sent":1,...}
[main] Upload OK (1725 ms)
```

### Common issues

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Red LED blinking | WiFi can't connect | Check SSID/password in `data/config.json`, re-upload with `pio run -t uploadfs` |
| Upload fails | Wrong USB port | `pio device list`, then `pio run -t upload --upload-port /dev/ttyUSB0` |
| Recording but no upload | Server unreachable | Check `server_host` in `data/config.json` |
| Upload timeout (ESP32 says failed but audio played) | HA response too slow | Known benign issue — audio was delivered, retry logic handles it |
| Upload OK but no sound | Wrong room key | Verify `room` in `data/config.json` matches a key in `rooms.json` (`study`/`living`/`cinema`/`bedroom`) |
| Config not loading | LittleFS not flashed | Run `pio run -t uploadfs` to upload the file system |
| PSRAM allocation warning | Board variant mismatch | Verify `board_build.psram_type = opi` in `platformio.ini` |

### Inspect build details

```bash
# Show all compiler flags
pio run -v -e esp32-s3-devkitc-1 2>&1 | grep -E "^-D|^-I|^-m"

# Show memory usage
pio run -e esp32-s3-devkitc-1 -t size

# Clean rebuild
pio run -e esp32-s3-devkitc-1 -t clean && pio run -e esp32-s3-devkitc-1
```

## License

MIT
