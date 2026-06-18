#pragma once
#include "WString.h"
#include <cstring>
#include <map>
#include <string>

/// Mock ESP32 Preferences (NVS) backed by an in-memory std::map.
/// One namespace = one map. Public access for test assertion.

class Preferences {
public:
    bool begin(const char* name, bool readOnly) {
        _ns = name;
        _readOnly = readOnly;
        return _stores.find(_ns) != _stores.end() || true; // always succeeds in mock
    }

    void end() {
        // no-op
    }

    String getString(const char* key, const char* defaultValue = "") const {
        auto ns_it = _stores.find(_ns);
        if (ns_it == _stores.end())
            return String(defaultValue);

        auto kv_it = ns_it->second.find(key);
        if (kv_it == ns_it->second.end())
            return String(defaultValue);

        return String(kv_it->second.c_str());
    }

    size_t putString(const char* key, const char* value) {
        if (_readOnly)
            return 0;
        _stores[_ns][key] = value;
        return strlen(value);
    }

    bool clear() {
        if (_readOnly)
            return false;
        _stores[_ns].clear();
        return true;
    }

    // ── Test helpers ──────────────────────────────

    /// Reset ALL mock storage (between tests).
    static void reset_all() {
        _stores.clear();
    }

    /// Direct access for test assertions.
    static std::map<std::string, std::string>& store(const char* ns) {
        return _stores[ns];
    }

private:
    std::string _ns;
    bool _readOnly = false;

    static inline std::map<std::string, std::map<std::string, std::string>> _stores;
};
