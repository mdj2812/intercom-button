#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <string>

/// Minimal Arduino String mock — wraps std::string.
class String {
    std::string _s;

public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const String&) = default;
    String(const std::string& s) : _s(s) {}

    const char* c_str() const {
        return _s.c_str();
    }
    unsigned int length() const {
        return static_cast<unsigned int>(_s.length());
    }
    int indexOf(const char* needle) const {
        auto pos = _s.find(needle);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    int indexOf(char c) const {
        auto pos = _s.find(c);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    bool operator==(const String& o) const {
        return _s == o._s;
    }
    bool operator!=(const String& o) const {
        return _s != o._s;
    }
    void toLowerCase() {
        for (char& c : _s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    void toUpperCase() {
        for (char& c : _s)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
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
    String& operator+=(const char* s) {
        _s += s ? s : "";
        return *this;
    }

    bool concat(const char* s) {
        if (!s)
            return false;
        _s += s;
        return true;
    }
    bool concat(const String& s) {
        _s += s._s;
        return true;
    }
    bool concat(char c) {
        _s += c;
        return true;
    }
    bool concat(unsigned int n) {
        _s += std::to_string(n);
        return true;
    }

    void trim() {
        auto not_space = [](unsigned char c) { return !std::isspace(c); };
        while (!_s.empty() && !not_space(static_cast<unsigned char>(_s.front())))
            _s.erase(_s.begin());
        while (!_s.empty() && !not_space(static_cast<unsigned char>(_s.back())))
            _s.pop_back();
    }

    bool startsWith(const char* prefix) const {
        if (!prefix)
            return false;
        return _s.rfind(prefix, 0) == 0;
    }

    String substring(unsigned int from) const {
        if (from >= _s.size())
            return String("");
        return String(_s.substr(from));
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from >= _s.size() || to <= from)
            return String("");
        return String(_s.substr(from, to - from));
    }

    int toInt() const {
        return std::atoi(_s.c_str());
    }

    String operator+(const String& o) const {
        String r(*this);
        r += o;
        return r;
    }
    String operator+(const char* o) const {
        String r(*this);
        r += o;
        return r;
    }

    bool operator==(const char* o) const {
        return _s == (o ? o : "");
    }
    bool operator!=(const char* o) const {
        return !(*this == o);
    }
};
