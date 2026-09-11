#pragma once
#include <cstddef>
#include <cstdint>

class UpdateClass {
public:
    bool begin(size_t) {
        return false;
    }
    size_t write(const uint8_t*, size_t) {
        return 0;
    }
    bool end() {
        return false;
    }
    void abort() {}
    const char* errorString() const {
        return "Update mock";
    }
};

inline UpdateClass Update;
