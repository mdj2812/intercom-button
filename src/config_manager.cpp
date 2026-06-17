#include "config_manager.h"
#include "room_target_store.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* TAG = "cfg";
// JSON document size for config.json with up to 8 buttons.
// Base fields (~200B) + 8× "pins" ints (~48B) + 8× "buttons" entries (~240B) + overhead.
static constexpr size_t JSON_DOC_SIZE = 768;

// ── Default values (used when config.json is missing) ─
struct Config {
    String wifi_ssid = "your_wifi_ssid";
    String wifi_password = "your_wifi_password";
    String server_host = "192.168.99.10";
    uint16_t server_port = 8764;
    String room = "study";
    uint32_t sample_rate = 16000;
    uint32_t max_secs = 60;

    // Per-button room mappings from config.json "buttons" field
    static constexpr uint8_t MAX_BUTTONS = 8;
    uint8_t button_pins[MAX_BUTTONS] = {};
    String button_rooms[MAX_BUTTONS];
    uint8_t button_count = 0;

    // Config-file pin list from "pins" field
    uint8_t pins[MAX_BUTTONS] = {};
    uint8_t pin_count = 0;
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

    // ── Parse button pin list ──────────────────────
    if (doc.containsKey("pins")) {
        JsonArray pinsArr = doc["pins"].as<JsonArray>();
        cfg.pin_count = 0;
        for (JsonVariant v : pinsArr) {
            if (cfg.pin_count >= Config::MAX_BUTTONS)
                break;
            cfg.pins[cfg.pin_count++] = v.as<uint8_t>();
        }
    }

    // ── Parse per-button room mappings ────────────
    if (doc.containsKey("buttons")) {
        JsonObject buttons = doc["buttons"].as<JsonObject>();
        cfg.button_count = 0;
        for (JsonPair kv : buttons) {
            if (cfg.button_count >= Config::MAX_BUTTONS)
                break;
            cfg.button_pins[cfg.button_count] = atoi(kv.key().c_str());
            cfg.button_rooms[cfg.button_count] = kv.value().as<String>();
            cfg.button_count++;
        }
    }

    // ── Validate: warn if pins/buttons sizes mismatch ─
    if (cfg.pin_count > 0 && cfg.button_count > 0 && cfg.pin_count != cfg.button_count) {
        Serial.printf("[%s] WARNING: pins count (%u) != buttons count (%u)\n", TAG, cfg.pin_count, cfg.button_count);
    }

    Serial.printf("[%s] Loaded: room=%s server=%s:%u wifi=%s buttons=%u pins=%u\n", TAG, cfg.room.c_str(),
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
const char* ConfigManager::server_host() {
    return cfg.server_host.c_str();
}
uint16_t ConfigManager::server_port() {
    return cfg.server_port;
}
const char* ConfigManager::room_target() {
    return cfg.room.c_str();
}
uint32_t ConfigManager::sample_rate() {
    return cfg.sample_rate;
}
uint32_t ConfigManager::max_record_secs() {
    return cfg.max_secs;
}

// ── Button defaults ────────────────────────────────

void ConfigManager::load_button_defaults(RoomTargetStore& store) {
    store.clear_defaults();
    for (uint8_t i = 0; i < cfg.button_count; i++) {
        store.set_default_room(cfg.button_pins[i], cfg.button_rooms[i].c_str());
    }
}

// ── Pin accessors ───────────────────────────────────

const uint8_t* ConfigManager::button_pins() {
    return cfg.pins;
}

uint8_t ConfigManager::button_pin_count() {
    return cfg.pin_count;
}
