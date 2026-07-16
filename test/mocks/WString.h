#pragma once
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cctype>

/// Minimal Arduino String mock — wraps std::string.
class String {
    std::string _s;

public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const String&) = default;
    String(const std::string& s) : _s(s) {}

    const char* c_str() const { return _s.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(_s.length()); }
    int indexOf(const char* needle) const {
        auto pos = _s.find(needle);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator!=(const String& o) const { return _s != o._s; }
    void toLowerCase() {
        for (char& c : _s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    String& operator=(const char* s) {
        _s = s ? s : "";
        return *this;
    }
    String& operator=(const String& o) {
        _s = o._s;
        return *this;
    }
    String& operator+=(const String& o) {
        _s += o._s;
        return *this;
    }

    bool concat(const char* s) {
        if (!s) return false;
        _s += s;
        return true;
    }
    bool concat(const String& s) {
        _s += s._s;
        return true;
    }
};
