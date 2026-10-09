#pragma once

#ifdef __cplusplus
#include <cstddef>

namespace neondoll {

class Platform {
public:
    virtual ~Platform() = default;
    virtual void init() = 0;
    virtual void deinit() = 0;
};

} // namespace neondoll
#endif // __cplusplus