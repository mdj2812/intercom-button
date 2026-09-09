#pragma once
#include "consts.hpp"
#include <cstdint>
#include <string>
#include <vector>

/// Per-GPIO-pin → room key mapping.
///
/// Priority chain (first match wins):
///   1. NVS (`btn_cfg` namespace, `btn_<gpio>` key) — runtime, filled by GET /rooms
///   2. In-memory defaults (set via `set_default_room()`)
///   3. Compiled-in hardcoded defaults
///
/// Key format: `btn_<gpio>` → room key string (e.g. "study", "living").
class RoomTargetStore {
public:
    /// Validate NVS accessibility. Opens and closes the namespace immediately
    /// — a one-shot health check, not a persistent handle. Call once in setup().
    bool begin();

    /// Look up the room target for a given GPIO pin.
    /// Priority: NVS → in-memory defaults → hardcoded defaults.
    std::string get_room(uint8_t gpio_pin) const;

    /// Store a room mapping in NVS. Takes effect immediately.
    bool set_room(uint8_t gpio_pin, const std::string& room);

    /// Erase all per-button room mappings from NVS (reset to defaults).
    void reset();

    /// Set an in-memory default. Enforces MAX_BUTTONS limit.
    /// These sit between NVS and hardcoded defaults.
    void set_default_room(uint8_t gpio, const std::string& room);

    /// Clear all in-memory defaults (e.g. before reloading).
    void clear_defaults();

    /// Number of in-memory defaults stored.
    uint8_t defaults_count() const;

private:
    static std::string _hardcoded_room(uint8_t gpio_pin);

    struct DefaultEntry {
        uint8_t gpio;
        std::string room;
    };

    std::vector<DefaultEntry> _defaults;
};
