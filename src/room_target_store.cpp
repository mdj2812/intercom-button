#include "room_target_store.h"
#include <cstdio>
#include <Preferences.h>
#include <string>

static const char* NVS_NS = "btn_cfg";

// ── Hardcoded ultimate fallback ─────────────────────
// These are used only when neither NVS nor config-file defaults match.

static const struct {
    uint8_t gpio;
    const char* room;
} HARDCODED[] = {
    {4, "study"},
    {5, "living"},
    {12, "cinema"},
    {13, "bedroom"},
};

static constexpr size_t HARDCODED_COUNT = sizeof(HARDCODED) / sizeof(HARDCODED[0]);

std::string RoomTargetStore::_hardcoded_room(uint8_t gpio_pin) {
    for (size_t i = 0; i < HARDCODED_COUNT; i++) {
        if (HARDCODED[i].gpio == gpio_pin)
            return std::string(HARDCODED[i].room);
    }
    return "study";
}

// ── Config-file defaults ────────────────────────────

void RoomTargetStore::set_default_room(uint8_t gpio, const std::string& room) {
    if (_defaults.size() >= MAX_BUTTONS)
        return;
    _defaults.push_back({gpio, room});
}

void RoomTargetStore::clear_defaults() {
    _defaults.clear();
}

uint8_t RoomTargetStore::defaults_count() const {
    return static_cast<uint8_t>(_defaults.size());
}

// ── NVS operations ──────────────────────────────────

bool RoomTargetStore::begin() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return false;
    }
    prefs.end();
    return true;
}

std::string RoomTargetStore::get_room(uint8_t gpio_pin) const {
    // Tier 1 — NVS (runtime override)
    Preferences prefs;
    if (prefs.begin(NVS_NS, true)) {
        std::string key = "btn_" + std::to_string(gpio_pin);

        String room = prefs.getString(key.c_str(), "");
        prefs.end();

        if (room.length() > 0) {
            return std::string(room.c_str());
        }
    }

    // Tier 2 — Config-file defaults
    for (const auto& d : _defaults) {
        if (d.gpio == gpio_pin)
            return d.room;
    }

    // Tier 3 — Hardcoded fallback
    return _hardcoded_room(gpio_pin);
}

bool RoomTargetStore::set_room(uint8_t gpio_pin, const std::string& room) {
    std::string truncated = room;
    if (truncated.length() > MAX_ROOM_KEY_LEN) {
        truncated.resize(MAX_ROOM_KEY_LEN);
        fprintf(stderr, "[room_store] WARNING: room name truncated to %u chars (was %u)\n", MAX_ROOM_KEY_LEN,
                (unsigned) room.length());
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return false;
    }

    std::string key = "btn_" + std::to_string(gpio_pin);
    size_t written = prefs.putString(key.c_str(), truncated.c_str());
    prefs.end();
    return written > 0;
}

void RoomTargetStore::reset() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return;
    }
    prefs.clear();
    prefs.end();
}
