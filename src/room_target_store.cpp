#include "room_target_store.h"
#include <Preferences.h>
#include <cstring>

static const char* NVS_NS = "btn_cfg";
static char _room_buf[32]; // cached result — valid until next get_room() call

// ── Compiled-in defaults ────────────────────────────
// Matches the button pins defined in config.h.
// Update when pins change.

struct DefaultEntry {
    uint8_t gpio;
    const char* room;
};

static const DefaultEntry DEFAULTS[] = {
    {4, "study"},
    {5, "living"},
    {12, "cinema"},
    {13, "bedroom"},
};

static constexpr size_t DEFAULT_COUNT = sizeof(DEFAULTS) / sizeof(DEFAULTS[0]);

const char* RoomTargetStore::_default_room(uint8_t gpio_pin) {
    for (size_t i = 0; i < DEFAULT_COUNT; i++) {
        if (DEFAULTS[i].gpio == gpio_pin)
            return DEFAULTS[i].room;
    }
    return "study"; // ultimate fallback
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
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) {
        return _default_room(gpio_pin);
    }

    char key[16];
    snprintf(key, sizeof(key), "btn_%u", gpio_pin);

    String room = prefs.getString(key, "");
    prefs.end();

    if (room.length() > 0) {
        strncpy(_room_buf, room.c_str(), sizeof(_room_buf) - 1);
        _room_buf[sizeof(_room_buf) - 1] = '\0';
        return _room_buf;
    }

    return _default_room(gpio_pin);
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
