#include "wifi_manager.h"

namespace WiFiManager {

static const char* TAG = "wifi";

static unsigned long next_reconnect_ms = 0;
static const unsigned long RECONNECT_INTERVAL = 5000; // retry every 5s

void begin(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
    Serial.printf("[%s] Connecting to %s...\n", TAG, ssid);
}

bool update() {
    if (WiFi.status() == WL_CONNECTED) {
        next_reconnect_ms = 0; // reset backoff once connected
        return true;
    }

    // Non-blocking: only retry at intervals
    unsigned long now = millis();
    if (next_reconnect_ms == 0 || now >= next_reconnect_ms) {
        next_reconnect_ms = now + RECONNECT_INTERVAL;
        Serial.printf("[%s] Reconnecting (status=%d)...\n", TAG, WiFi.status());
        WiFi.reconnect();
    }
    return false;
}

int rssi() {
    if (WiFi.status() != WL_CONNECTED)
        return 0;
    return WiFi.RSSI();
}

const char* status_str() {
    switch (WiFi.status()) {
        case WL_CONNECTED:
            return "connected";
        case WL_NO_SSID_AVAIL:
            return "no_ssid";
        case WL_CONNECT_FAILED:
            return "connect_failed";
        case WL_IDLE_STATUS:
            return "idle";
        case WL_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
    }
}

} // namespace WiFiManager
