#include "config_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char *TAG = "cfg";

// ── Default values (used when config.json is missing) ─
struct Config {
    String wifi_ssid     = "your_wifi_ssid";
    String wifi_password = "your_wifi_password";
    String server_host   = "192.168.99.10";
    uint16_t server_port = 8764;
    String room          = "study";
    uint32_t sample_rate  = 16000;
    uint32_t max_secs     = 60;
};
static Config cfg;

bool ConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.printf("[%s] LittleFS mount failed — using defaults\n", TAG);
        return false;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.printf("[%s] /config.json not found — using defaults\n", TAG);
        Serial.printf("[%s] Upload with: pio run -t uploadfs\n", TAG);
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[%s] JSON parse error: %s — using defaults\n",
                      TAG, err.c_str());
        return false;
    }

    // ── Read fields (keep defaults for missing keys) ──
    if (doc.containsKey("wifi_ssid"))
        cfg.wifi_ssid = doc["wifi_ssid"].as<String>();
    if (doc.containsKey("wifi_password"))
        cfg.wifi_password = doc["wifi_password"].as<String>();
    if (doc.containsKey("server_host"))
        cfg.server_host = doc["server_host"].as<String>();
    if (doc.containsKey("server_port"))
        cfg.server_port = doc["server_port"].as<uint16_t>();
    if (doc.containsKey("room"))
        cfg.room = doc["room"].as<String>();
    if (doc.containsKey("sample_rate"))
        cfg.sample_rate = doc["sample_rate"].as<uint32_t>();
    if (doc.containsKey("max_record_secs"))
        cfg.max_secs = doc["max_record_secs"].as<uint32_t>();

    Serial.printf("[%s] Loaded: room=%s server=%s:%u wifi=%s\n",
                  TAG, cfg.room.c_str(), cfg.server_host.c_str(),
                  cfg.server_port, cfg.wifi_ssid.c_str());
    return true;
}

// ── Accessors ────────────────────────────────────────

const char* ConfigManager::wifi_ssid()     { return cfg.wifi_ssid.c_str(); }
const char* ConfigManager::wifi_password() { return cfg.wifi_password.c_str(); }
const char* ConfigManager::server_host()   { return cfg.server_host.c_str(); }
uint16_t    ConfigManager::server_port()   { return cfg.server_port; }
const char* ConfigManager::room_target()   { return cfg.room.c_str(); }
uint32_t    ConfigManager::sample_rate()   { return cfg.sample_rate; }
uint32_t    ConfigManager::max_record_secs(){ return cfg.max_secs; }
