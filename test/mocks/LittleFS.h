#pragma once
#include "FS.h"
#include <string>
#include <map>

/// Mock LittleFS — uses an in-memory map (<path, content>) for file injection.
class LittleFSClass {
    std::map<std::string, std::string> _files;
    bool _mounted = false;

public:
    bool begin(bool formatOnFail = false) {
        (void)formatOnFail;
        _mounted = true;
        return true;
    }

    File open(const char* path, const char* mode) {
        (void)mode;
        if (!_mounted) return File();
        auto it = _files.find(path);
        if (it == _files.end()) return File();
        return File(it->second);
    }

    // ── Test hooks ───────────────────────────────────
    void inject(const char* path, const std::string& content) {
        _files[path] = content;
    }
    void reset() {
        _files.clear();
        _mounted = false;
    }
};

inline LittleFSClass LittleFS;
