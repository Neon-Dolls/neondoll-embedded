#pragma once
#include <string>

namespace neondoll {

class Terminal {
public:
    virtual ~Terminal() = default;
    // For M0, we do not implement terminal functionality.
    // We leave this as a placeholder for the interface.
    virtual bool init() { return true; }
    virtual std::string read_line() { return ""; }
    virtual void write_line(const std::string& line) {}
};

} // namespace neondoll