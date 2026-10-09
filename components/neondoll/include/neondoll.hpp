#pragma once

#ifdef __cplusplus
#include <cstddef>
#include "neondoll/platform.hpp"
extern "C" {
#endif

void neondoll_init(void);
void neondoll_deinit(void);

#ifdef __cplusplus
} // extern "C"

namespace neondoll {

void set_platform(Platform* platform);

} // namespace neondoll
#endif // __cplusplus