# Changelog

## [Unreleased]

### 🚀 Added

- Native tests for `audio_recorder.cpp` and `main.cpp` setup/loop (#8). ESP timer/ADC/heap/`Update`/mbedtls are mocked; `ota_manager.cpp` is the real module.

### 🐛 Fixed

- **Signed OTA stack overflow** — download used a 4KB stack buffer, then mbedtls ECDSA ran on the default 8KB `loopTask` and paniced (`Stack canary watchpoint triggered`). Chunk buffer is on the heap; loop stack is 24KB.
- **HIL confirm_test vs leftover `ota`** — deapprove the reserved MAC and cancel leftover `ota_requested` before USB flash/dry-run so a pending PWA Update cannot start OTA while serial expects idle.

### 🔧 Changed

- Gitea HIL repo variables name the board and LAN server (`HIL_MAC`, `HIL_SSH_HOST`, `HIL_CONTAINER`, …), not a specific appliance. SSH identity and `docker` path stay on the runner. Required HIL env has no lab defaults in the repo.

## [0.3.0] — 2026-09-10

### ⚠ Breaking

- **Room map is hello `buttons` only (#39)** — the firmware no longer calls `GET /api/home_intercom/rooms` or maps `pin[i]` → catalog key `[i]`. Bind GPIOs in the Home Intercom PWA. Empty hello `{}` keeps last NVS; pins omitted from a non-empty map are unassigned and skip upload.
- **Audio is not in `config.json` (#29, #30)** — leftover `sample_rate` / `max_record_secs` are ignored. After WiFi, `GET /api/home_intercom/config` supplies them (hello repeats). Offline boot is 16000 Hz / 60 s. `config.json` is wifi / server / `pins` only (`ha_token` and `buttons` leftovers stay ignored).

### 🔧 Changed

- Hello POST includes `pins` so the PWA can bind GPIOs using `GET /media_players` (home-intercom#81). Idle hello heartbeats refresh the map so a PWA edit applies without reboot (home-intercom#74).
- Failed audio apply retries GET `/config` instead of treating the fetch as done.
- OTA SHA header match is case-insensitive (`X-Checksum-Sha256` from Waitress).
- Dev Docker image now preinstalls `tool-scons`, Adafruit NeoPixel, ArduinoJson, and Unity (global PIO packages, so a bind-mounted `/workspace` still sees them).
- Gitea HIL is three jobs: compile (no USB), flash, then serial/OTA/buttons test.

### 🏠 Home Intercom (related)

- Requires a server that serves `GET /api/home_intercom/config` and hello `buttons` / `pins` (home-intercom#78 / #81 / #83). Catalog-order `/rooms` mapping is not enough.

## [0.2.2] — 2026-09-10

### 🔧 Changed

- **OTA confirm via hello (#42)** — after an OTA reboot, a parsed `/devices/hello` (`ok` / `pending` / `revoked`) marks the image valid. Button and serial `confirm` remain shortcuts. No hello within 60s still rolls back. Unattended panels no longer need a 60s button press to keep a good update.
- Serial `confirm_test` / `confirm_test nohello` dry-runs that window (15s) without marking the image or rolling back.

## [0.2.1] — 2026-09-09

### 🚀 Added

- **Server-triggered OTA** — idle hello with `"ota": true` downloads `GET /api/home_intercom/firmware` over LAN HTTP (GitHub release assets are HTTPS-only). After flash, press any button (or serial `confirm`) within 60s or the image rolls back.
- **Release signatures** — CI signs the `.bin` with repo secret `OTA_PRIVATE_KEY` and uploads a 64-byte `intercom-button-vX.Y.Z.bin.sig`. A missing `.sig` still flashes using SHA-256 only.

### 🔧 Changed

- OTA public key replaced. Boards still on the previous key must take this image via serial or **unsigned** OTA before they will accept a signed GitHub `.sig`.

### 🏠 Home Intercom (related)

- Requires a server that caches the GitHub `.bin` and sets hello `ota: true` (home-intercom server-triggered OTA).

## [0.2.0] — 2026-09-09

### 🔒 Security

- **MAC-based device identity (#31)** — uploads and OTA requests send `X-Device-ID: <STA MAC>` and POST audio to `/api/home_intercom/device/record`. `ha_token` is no longer read or sent; a leftover key in existing `config.json` is ignored. HTTP 401/403 are treated as auth failures (no retry, no assumed delivery).
- **Trust-on-first-use hello (#32)** — after WiFi connects, `POST /api/home_intercom/devices/hello` registers the MAC. Recording waits until hello succeeds (orange LED). While idle, hello repeats every 10 seconds to refresh HA last_seen. Revoked/pending heartbeats return to the orange wait; a network blip retries in 30s without dropping registration. Audio `max_record_secs` from the payload is applied.

### 🔧 Changed

- **Server-driven room map (#28, #40)** — after a successful hello, `GET /api/home_intercom/rooms` assigns `pin[i]` → catalog key `[i]` (JSON document order) into NVS. Fetch failure keeps the last NVS map (then the compiled GPIO→room fallback). Optional `pins` array; leftover `buttons` in `config.json` is ignored.
- **Drop `buttons` from config** — GPIO→room is no longer set in `config.json`. Hardware GPIOs stay in `pins`; room keys come from the server.
- Revoked hello retries every 30 seconds.
- Pending pairing blinks orange on a side task so blocking HTTP still flashes.

### 📖 Documentation

- Parametric printable enclosure (OpenSCAD) with BOM and CI-generated STLs (#35)
- Demo video/GIF in the README (#33)

### 🧪 Tests

- Native `RoomFetcher` tests for catalog parse and pin-index apply
- Leftover `buttons` object in config is ignored

### 🏠 Home Intercom (related)

- Requires a server that serves `POST /api/home_intercom/devices/hello` and `GET /api/home_intercom/rooms` (home-intercom v2.1.x).
- Per-device GPIO → room map (not catalog order) is [home-intercom#78](https://github.com/mdj2812/home-intercom/issues/78) / [#39](https://github.com/mdj2812/intercom-button/issues/39).

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
