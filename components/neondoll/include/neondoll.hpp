#pragma once

#ifdef __cplusplus
#include <cstddef>
#include "platform.hpp"

namespace neondoll {

void set_platform(Platform* platform);
void init(void);
void deinit(void);

} // namespace neondoll
#endif // __cplusplus