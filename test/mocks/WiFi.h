#pragma once
#include <Arduino.h>

// ── WiFi status codes ───────────────────────────────
#define WL_NO_SHIELD       255
#define WL_IDLE_STATUS     0
#define WL_NO_SSID_AVAIL   1
#define WL_SCAN_COMPLETED  2
#define WL_CONNECTED       3
#define WL_CONNECT_FAILED  4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED    6

#define WIFI_STA 1
#define WIFI_AP  2

// ── Mockable WiFi state ─────────────────────────────
struct WiFiState {
    int status = WL_DISCONNECTED;
    int rssi = 0;
    bool auto_reconnect = false;
    const char* ssid = "";
    int mode = 0;
};
inline WiFiState _wifi_mock;

/// Reset WiFi mock to defaults (disconnected, no auto-reconnect).
inline void mock_wifi_reset() {
    _wifi_mock = WiFiState{};
}

/// Simulate WiFi connection for tests.
inline void mock_wifi_set_connected(bool connected, int rssi = -50) {
    _wifi_mock.status = connected ? WL_CONNECTED : WL_DISCONNECTED;
    _wifi_mock.rssi = connected ? rssi : 0;
}

// ── WiFi class ──────────────────────────────────────
class WiFiClass {
public:
    void mode(int m) { _wifi_mock.mode = m; }

    void setAutoReconnect(bool v) { _wifi_mock.auto_reconnect = v; }

    void begin(const char* ssid, const char* password) {
        (void)password;
        _wifi_mock.ssid = ssid;
        // don't auto-connect in mock — tests control state
    }

    int status() { return _wifi_mock.status; }
    void reconnect() { /* tests control connection via mock_wifi_set_connected */ }
    int RSSI() { return _wifi_mock.rssi; }
};

inline WiFiClass WiFi;
