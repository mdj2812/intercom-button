#pragma once
#include <Stream.h>
#include <string>
#include <cstdint>
#include <cstring>

/// Mock File class — inherits Stream so ArduinoJson can deserializeJson() from it.
class File : public Stream {
    std::string _content;
    size_t _pos = 0;
    bool _open = false;

public:
    File() = default;
    explicit File(const std::string& content) : _content(content), _open(true) {}

    // ── Stream overrides ─────────────────────────────
    int available() override {
        if (!_open) return 0;
        return static_cast<int>(_content.size() - _pos);
    }
    int read() override {
        if (!_open || _pos >= _content.size()) return -1;
        return static_cast<uint8_t>(_content[_pos++]);
    }
    int peek() override {
        if (!_open || _pos >= _content.size()) return -1;
        return static_cast<uint8_t>(_content[_pos]);
    }

    size_t read(uint8_t* buf, size_t len) {
        if (!_open) return 0;
        size_t avail = _content.size() - _pos;
        size_t n = (len < avail) ? len : avail;
        memcpy(buf, _content.data() + _pos, n);
        _pos += n;
        return n;
    }

    // ── Status ───────────────────────────────────────
    operator bool() const { return _open; }
    void close() { _open = false; _pos = 0; }

    // ── Test hooks ───────────────────────────────────
    bool isOpen() const { return _open; }
};
