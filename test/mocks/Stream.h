#pragma once
#include "Print.h"

/// Minimal Stream class (inherits Print). Used as base for hardware serial.
class Stream : public Print {
public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
};
