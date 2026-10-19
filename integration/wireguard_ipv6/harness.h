#ifndef ND_WG_HARNESS_H
#define ND_WG_HARNESS_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int nd_setup(const char *address, const char *prefix, const char *mask);
void nd_cleanup(void);
int nd_transmit(const uint8_t *data, size_t length, int chained);
int nd_receive(const uint8_t *data, size_t length, int tamper, size_t padding);
size_t nd_received(uint8_t *out);
int nd_receive_wire(void);
int nd_outer_ipv4(void);
int nd_default_unchanged(void);
int nd_address_correct(const char *address);
int nd_output_registered(void);
int nd_add_allowed(const char *prefix, const char *mask);
#ifdef __cplusplus
}
#endif
#endif
