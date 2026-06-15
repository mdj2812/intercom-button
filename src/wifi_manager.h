#pragma once
#include <WiFi.h>

/// Non-blocking WiFi manager with auto-reconnect and RSSI polling.
/// Call wifi_update() every loop() iteration.
namespace WiFiManager {

/// Initialize WiFi in station mode (call once in setup()).
/// Returns immediately — connection runs in background.
void begin(const char* ssid, const char* password);

/// Call every loop() to drive reconnection logic.
/// Returns true when connected.
bool update();

/// Returns current RSSI in dBm, or 0 if disconnected.
int rssi();

/// True if IP acquired and WiFi is ready for HTTP.
inline bool is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

/// Human-readable connection status for serial logging.
const char* status_str();

} // namespace WiFiManager
