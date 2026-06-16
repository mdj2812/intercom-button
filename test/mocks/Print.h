#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "Printable.h"
#include "WString.h"

/// Minimal Arduino Print base class.
class Print {
public:
    virtual ~Print() = default;

    virtual size_t write(uint8_t c) {
        putchar(c);
        return 1;
    }
    virtual size_t write(const uint8_t* buf, size_t len) {
        size_t n = 0;
        while (len--) { write(*buf++); n++; }
        return n;
    }

    size_t print(const char* s) {
        if (!s) return 0;
        size_t n = 0;
        while (*s) { write(*s++); n++; }
        return n;
    }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(int n) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "%d", n);
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }
    size_t print(unsigned int n) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "%u", n);
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }
    size_t print(long n) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "%ld", n);
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }
    size_t print(unsigned long n) {
        char buf[24];
        int len = snprintf(buf, sizeof(buf), "%lu", n);
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }
    size_t print(float f, int decimals = 2) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%.*f", decimals, (double)f);
        return write(reinterpret_cast<const uint8_t*>(buf), len);
    }

    size_t println() { return write('\n'); }
    size_t println(const char* s) { size_t n = print(s); n += println(); return n; }
    size_t println(const String& s) { return println(s.c_str()); }
    size_t println(int n) { size_t a = print(n); a += println(); return a; }
    size_t println(unsigned int n) { size_t a = print(n); a += println(); return a; }
    size_t println(long n) { size_t a = print(n); a += println(); return a; }
    size_t println(unsigned long n) { size_t a = print(n); a += println(); return a; }

    int printf(const char* fmt, ...);
};
