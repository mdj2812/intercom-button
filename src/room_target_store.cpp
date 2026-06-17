#include "room_target_store.h"
#include <Preferences.h>
#include <cstring>

static const char* NVS_NS = "btn_cfg";
static char _room_buf[32]; // cached result — valid until next get_room() call

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

const char* RoomTargetStore::_hardcoded_room(uint8_t gpio_pin) {
    for (size_t i = 0; i < HARDCODED_COUNT; i++) {
        if (HARDCODED[i].gpio == gpio_pin)
            return HARDCODED[i].room;
    }
    return "study";
}

// ── Config-file defaults ────────────────────────────

void RoomTargetStore::set_default_room(uint8_t gpio, const char* room) {
    if (_defaults_count >= MAX_DEFAULTS)
        return;
    _defaults[_defaults_count].gpio = gpio;
    strncpy(_defaults[_defaults_count].room, room, sizeof(_defaults[0].room) - 1);
    _defaults[_defaults_count].room[sizeof(_defaults[0].room) - 1] = '\0';
    _defaults_count++;
}

void RoomTargetStore::clear_defaults() {
    _defaults_count = 0;
}

uint8_t RoomTargetStore::defaults_count() const {
    return _defaults_count;
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

const char* RoomTargetStore::get_room(uint8_t gpio_pin) const {
    // Tier 1 — NVS (runtime override)
    Preferences prefs;
    if (prefs.begin(NVS_NS, true)) {
        char key[16];
        snprintf(key, sizeof(key), "btn_%u", gpio_pin);

        String room = prefs.getString(key, "");
        prefs.end();

        if (room.length() > 0) {
            strncpy(_room_buf, room.c_str(), sizeof(_room_buf) - 1);
            _room_buf[sizeof(_room_buf) - 1] = '\0';
            return _room_buf;
        }
    }

    // Tier 2 — Config-file defaults
    for (uint8_t i = 0; i < _defaults_count; i++) {
        if (_defaults[i].gpio == gpio_pin)
            return _defaults[i].room;
    }

    // Tier 3 — Hardcoded fallback
    return _hardcoded_room(gpio_pin);
}

bool RoomTargetStore::set_room(uint8_t gpio_pin, const char* room) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "btn_%u", gpio_pin);

    size_t written = prefs.putString(key, room);
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
