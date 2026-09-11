#pragma once
#include "WString.h"
#include <cstdint>
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

    bool isKey(const char* key) const {
        auto ns_it = _stores.find(_ns);
        if (ns_it == _stores.end())
            return false;
        return ns_it->second.find(key) != ns_it->second.end();
    }

    size_t putString(const char* key, const char* value) {
        if (_readOnly)
            return 0;
        _stores[_ns][key] = value;
        return strlen(value);
    }

    bool putBool(const char* key, bool value) {
        if (_readOnly)
            return false;
        _stores[_ns][key] = value ? "1" : "0";
        return true;
    }

    bool getBool(const char* key, bool defaultValue = false) const {
        auto ns_it = _stores.find(_ns);
        if (ns_it == _stores.end())
            return defaultValue;
        auto kv_it = ns_it->second.find(key);
        if (kv_it == ns_it->second.end())
            return defaultValue;
        return kv_it->second == "1" || kv_it->second == "true";
    }

    size_t putUInt(const char* key, uint32_t value) {
        if (_readOnly)
            return 0;
        _stores[_ns][key] = std::to_string(value);
        return 1;
    }

    uint32_t getUInt(const char* key, uint32_t defaultValue = 0) const {
        auto ns_it = _stores.find(_ns);
        if (ns_it == _stores.end())
            return defaultValue;
        auto kv_it = ns_it->second.find(key);
        if (kv_it == ns_it->second.end())
            return defaultValue;
        return static_cast<uint32_t>(std::stoul(kv_it->second));
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
