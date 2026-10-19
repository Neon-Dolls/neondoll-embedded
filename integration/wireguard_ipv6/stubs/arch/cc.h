#ifndef ND_ARCH_CC_H
#define ND_ARCH_CC_H
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#define LWIP_PLATFORM_DIAG(x) do {} while (0)
#define LWIP_PLATFORM_ASSERT(x) assert(0 && x)
#define LWIP_RAND() ((uint32_t)1)
#endif
