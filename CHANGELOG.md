# Changelog

## [0.1.0] — 2026-07-16

### 🚀 Added

- **HA authentication support** — `ha_token` field in `config.json`. When set, sends `Authorization: *** header with every upload. Empty = backward-compatible Docker mode.
- **HTTPS support** via `server_scheme` config field (`http`/`https`). Uses `WiFiClientSecure::setInsecure()` — traffic is encrypted but server certificate is not verified (see README security note).
- **Unified API path** — both Docker and HA modes use `/api/home_intercom/record`. The `Authorization` header distinguishes authenticated vs unauthenticated requests.

### 🔧 Changed

- **Config breaking change** — `room` and `pins` fields removed. Room targeting is now exclusively via the `buttons` mapping: `{"4": "study", "5": "living", ...}`.
- **Default port** changed from `8764` to `8123` (Home Assistant default).
- **`dev.sh`** no longer falls back to local build when image is not found. Pulls from `ghcr.io/mdj2812/intercom-button-dev:latest`; use `-b` to build locally.

### 📖 Documentation

- README config examples now include `buttons` mapping
- Security note about plaintext token storage on ESP32 flash
- Field description tables in both EN and CN READMEs

### 🧪 Tests

- 4 new auth tests: token present → header added, empty → no header, null → no header, HTTPS scheme + URL
- Updated URL assertions for new `/api/home_intercom/record` path
- 68/68 native tests passing

### 🏠 Home Intercom (related)

- [Milestone #4: Server-Driven Configuration](https://github.com/mdj2812/intercom-button/milestone/4) — future goal to remove all secrets from ESP32. Device identifies by MAC, server proxies HA auth.
- [home-intercom Milestone #3](https://github.com/mdj2812/home-intercom/milestone/3) — server-side `POST /devices/hello`, device registry, mac-based auth.

---

## [0.0.1] — 2026-06-15

### Added
- Push-to-talk desktop intercom button firmware for ESP32-S3
- MAX9814 microphone support with hardware AGC (50dB gain)
- State machine: IDLE → RECORDING → UPLOADING with LED feedback
- 16kHz 16-bit mono PCM recording via Timer ISR
- PSRAM ring buffer (up to 60 seconds of audio)
- Non-blocking WiFi connection with auto-reconnect
- Per-device JSON configuration via LittleFS (`data/config.json`)
- HTTP POST upload to Flask intercom backend (`/convert?target=<room>`)
- Retry logic for upload failures (3 attempts)
- WS2812 RGB LED status indicator
- PlatformIO build system with `native` test environment
- 6 unit tests (JSON config parsing + WAV header validation)
- Docker dev image with preheated toolchains (v1.0.6)
- Gitea Actions CI pipeline: build, static analysis (cppcheck), unit tests
- Makefile for convenient `make build/flash/test/check`

### Hardware
- Target: ESP32-S3-DevKitC (WROOM-1 N8, 8MB Flash, OPI PSRAM)
- Microphone: MAX9814 (ADC on GPIO1)
- Button: GPIO4 (active low, internal pull-up)
- LED: WS2812 on GPIO48
