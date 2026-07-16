# Changelog

## [0.1.0] — 2026-07-15

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
