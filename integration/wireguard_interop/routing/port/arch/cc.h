#ifndef ND_ROUTING_CC_H
#define ND_ROUTING_CC_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#define __containerof(ptr,type,member) ((type *)(void *)((char *)(ptr)-offsetof(type,member)))
#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/time.h>
#define LWIP_ERRNO_STDINCLUDE 1
#define LWIP_ERRNO_INCLUDE <errno.h>
#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { fprintf(stderr,"%s\n",x); abort(); } while (0)
#define LWIP_RAND() ((uint32_t)1)
#endif
