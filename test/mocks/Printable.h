#pragma once

class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(class Print& p) const = 0;
};
