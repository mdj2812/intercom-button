#pragma once
#include <cstdint>

/// Per-GPIO-pin → room key mapping stored in NVS.
///
/// Key format: `btn_<gpio>` → room key string (e.g. "study", "living").
/// Falls back to compiled-in defaults when NVS key is missing.
class RoomTargetStore {
public:
    /// Open NVS namespace. Must be called once in setup().
    bool begin();

    /// Look up the room target for a given GPIO pin.
    /// Returns compiled-in default if NVS read fails or key not found.
    const char* get_room(uint8_t gpio_pin) const;

    /// Store a room mapping in NVS. Takes effect immediately.
    bool set_room(uint8_t gpio_pin, const char* room);

    /// Erase all per-button room mappings from NVS (reset to defaults).
    void reset();

private:
    static const char* _default_room(uint8_t gpio_pin);
};
