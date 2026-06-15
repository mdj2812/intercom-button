# Changelog

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
