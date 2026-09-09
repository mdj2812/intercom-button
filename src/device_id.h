#pragma once

/// Device identity for server auth. The ESP32 holds no secrets — the STA MAC
/// is the identifier sent as X-Device-ID on every API call.
namespace DeviceId {

/// WiFi STA MAC, uppercase colon-separated (e.g. "AA:BB:CC:DD:EE:FF").
/// Safe to call after WiFi.mode(WIFI_STA); does not require an association.
const char* mac();

} // namespace DeviceId
