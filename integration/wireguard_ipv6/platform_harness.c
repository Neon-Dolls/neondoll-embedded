// Compile the real patched platform lifecycle, with fault-injected mbedTLS API
// boundaries. These tests verify ownership/erasure, not DRBG cryptography.
#include <string.h>
#include "wireguard-platform.c"
static int fault, seeds;
void mbedtls_entropy_init(mbedtls_entropy_context *p) { memset(p,0x33,sizeof(*p)); }
void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *p) { memset(p,0x44,sizeof(*p)); }
void mbedtls_entropy_free(mbedtls_entropy_context *p) { (void)p; }
void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *p) { (void)p; }
int mbedtls_entropy_func(void *p,unsigned char *out,size_t n) { (void)p;memset(out,1,n);return 0; }
int mbedtls_entropy_add_source(mbedtls_entropy_context *p,int (*f)(void *,unsigned char *,size_t,size_t *),void *a,size_t t,int strong) {
    (void)p;(void)f;(void)a;(void)t;(void)strong;return fault==1?-1:0;
}
int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *p,int (*f)(void *,unsigned char *,size_t),void *a,const unsigned char *s,size_t n) {
    (void)p;(void)f;(void)a;(void)s;(void)n;++seeds;return fault==2?-1:0;
}
int mbedtls_ctr_drbg_random(void *p,unsigned char *out,size_t n) { (void)p;memset(out,0x55,n);return fault==3?-1:0; }
void esp_fill_random(void *out,size_t n) { memset(out,0x11,n); }
u32_t sys_now(void) { return 1000; }
void nd_platform_fault(int value) { fault=value; }
int nd_platform_seeds(void) { return seeds; }
int nd_platform_cleared(void) {
    if(platform_initialized)return 0;
    const unsigned char *p=(void *)&random_context;for(size_t i=0;i<sizeof(random_context);++i)if(p[i])return 0;
    p=(void *)&entropy_context;for(size_t i=0;i<sizeof(entropy_context);++i)if(p[i])return 0;
    return 1;
}
