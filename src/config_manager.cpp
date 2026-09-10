#include "config_manager.h"
#include "consts.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* TAG = "cfg";

// JSON key constants
static constexpr const char* KEY_WIFI_SSID = "wifi_ssid";
static constexpr const char* KEY_WIFI_PASSWORD = "wifi_password";
static constexpr const char* KEY_SERVER_SCHEME = "server_scheme";
static constexpr const char* KEY_SERVER_HOST = "server_host";
static constexpr const char* KEY_SERVER_PORT = "server_port";
static constexpr const char* KEY_PINS = "pins";

// JSON document size: base fields + pin array + slack for unknown keys
static constexpr size_t JSON_BASE = 256;   // wifi, server fields
static constexpr size_t JSON_PER_PIN = 50; // pin entry + leftover-key slack
static constexpr size_t JSON_DOC_SIZE = JSON_BASE + MAX_BUTTONS * JSON_PER_PIN;

// ── Default values (used when config.json is missing) ─
struct Config {
    String wifi_ssid = "your_wifi_ssid";
    String wifi_password = "your_wifi_password";
    String server_scheme = "http";
    String server_host = "192.168.99.4";
    uint16_t server_port = 8123;

    // Hardware pins from "pins"
    uint8_t button_pins[MAX_BUTTONS] = {};
    uint8_t button_count = 0;
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

    StaticJsonDocument<JSON_DOC_SIZE> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[%s] JSON parse error: %s — using defaults\n", TAG, err.c_str());
        return false;
    }

    // ── Read fields (keep defaults for missing keys) ──
    if (doc.containsKey(KEY_WIFI_SSID))
        cfg.wifi_ssid = doc[KEY_WIFI_SSID].as<String>();
    if (doc.containsKey(KEY_WIFI_PASSWORD))
        cfg.wifi_password = doc[KEY_WIFI_PASSWORD].as<String>();
    if (doc.containsKey(KEY_SERVER_SCHEME)) {
        String scheme = doc[KEY_SERVER_SCHEME].as<String>();
        scheme.toLowerCase();
        if (scheme == "http" || scheme == "https") {
            cfg.server_scheme = scheme;
        } else {
            Serial.printf("[%s] WARNING: unsupported server_scheme '%s' — keeping %s\n", TAG, scheme.c_str(),
                          cfg.server_scheme.c_str());
        }
    }
    if (doc.containsKey(KEY_SERVER_HOST))
        cfg.server_host = doc[KEY_SERVER_HOST].as<String>();
    if (doc.containsKey(KEY_SERVER_PORT))
        cfg.server_port = doc[KEY_SERVER_PORT].as<uint16_t>();

    // Leftover sample_rate / max_record_secs in config.json are ignored (#29).

    // ── Hardware pins ─────────────────────────────
    if (doc.containsKey(KEY_PINS) && doc[KEY_PINS].is<JsonArray>()) {
        JsonArray pins = doc[KEY_PINS].as<JsonArray>();
        cfg.button_count = 0;
        for (JsonVariant v : pins) {
            if (cfg.button_count >= MAX_BUTTONS)
                break;
            cfg.button_pins[cfg.button_count++] = v.as<uint8_t>();
        }
    }

    Serial.printf("[%s] Loaded: server=%s://%s:%u wifi=%s pins=%u\n", TAG, cfg.server_scheme.c_str(),
                  cfg.server_host.c_str(), cfg.server_port, cfg.wifi_ssid.c_str(), cfg.button_count);
    return true;
}

// ── Accessors ────────────────────────────────────────

const char* ConfigManager::wifi_ssid() {
    return cfg.wifi_ssid.c_str();
}
const char* ConfigManager::wifi_password() {
    return cfg.wifi_password.c_str();
}
const char* ConfigManager::server_scheme() {
    return cfg.server_scheme.c_str();
}
const char* ConfigManager::server_host() {
    return cfg.server_host.c_str();
}
uint16_t ConfigManager::server_port() {
    return cfg.server_port;
}

// ── Pin accessors ───────────────────────────────────

const uint8_t* ConfigManager::active_pins() {
    return cfg.button_pins;
}

uint8_t ConfigManager::active_pin_count() {
    return cfg.button_count;
}
