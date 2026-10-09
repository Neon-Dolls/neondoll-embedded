#pragma once

#ifdef __cplusplus
#include <cstddef>

namespace neondoll {

class Platform {
public:
    virtual ~Platform() = default;
    virtual void init() = 0;
    virtual void deinit() = 0;
    // Narrow platform interfaces for M0
    virtual void storage_init() = 0;
    virtual void entropy_init() = 0;
    virtual void clock_init() = 0;
    virtual void network_init() = 0;
    virtual void log_init() = 0;
};

} // namespace neondoll
#endif // __cplusplus