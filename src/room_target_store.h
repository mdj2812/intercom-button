#pragma once
#include <cstdint>

/// Per-GPIO-pin → room key mapping.
///
/// Priority chain (first match wins):
///   1. NVS (`btn_cfg` namespace, `btn_<gpio>` key) — runtime reconfigurable
///   2. Config-file defaults (set via `set_default_room()`, typically from config.json)
///   3. Compiled-in hardcoded defaults
///
/// Key format: `btn_<gpio>` → room key string (e.g. "study", "living").
class RoomTargetStore {
public:
    /// Open NVS namespace. Must be called once in setup().
    bool begin();

    /// Look up the room target for a given GPIO pin.
    /// Priority: NVS → config-file defaults → hardcoded defaults.
    const char* get_room(uint8_t gpio_pin) const;

    /// Store a room mapping in NVS. Takes effect immediately.
    bool set_room(uint8_t gpio_pin, const char* room);

    /// Erase all per-button room mappings from NVS (reset to defaults).
    void reset();

    /// Set a config-file-level default (typically from config.json "buttons" field).
    /// These sit between NVS and hardcoded defaults in the priority chain.
    void set_default_room(uint8_t gpio, const char* room);

    /// Clear all config-file defaults (e.g. before reloading).
    void clear_defaults();

    /// Number of config-file defaults stored.
    uint8_t defaults_count() const;

private:
    static const char* _hardcoded_room(uint8_t gpio_pin);

    struct DefaultEntry {
        uint8_t gpio;
        char room[32];
    };

    static constexpr uint8_t MAX_DEFAULTS = 8;
    DefaultEntry _defaults[MAX_DEFAULTS];
    uint8_t _defaults_count = 0;
};
