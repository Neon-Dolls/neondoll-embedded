#ifndef ND_ENTROPY_H
#define ND_ENTROPY_H
#include <stddef.h>
#define MBEDTLS_ENTROPY_SOURCE_STRONG 1
typedef struct mbedtls_entropy_context { unsigned char data[64]; } mbedtls_entropy_context;
void mbedtls_entropy_init(mbedtls_entropy_context *);
void mbedtls_entropy_free(mbedtls_entropy_context *);
int mbedtls_entropy_func(void *, unsigned char *, size_t);
int mbedtls_entropy_add_source(mbedtls_entropy_context *, int (*)(void *,unsigned char *,size_t,size_t *),void *,size_t,int);
#endif
