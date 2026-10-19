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
size_t nd_validate_ip6_length(const uint8_t *header, size_t bytes, int padded);
void nd_backend_reset(void);
void nd_backend_fault(const char *name);
void nd_backend_clock(uint32_t now);
void nd_backend_peer_keys(void);
int nd_backend_authenticate(int tamper);
int nd_backend_resources(void);
int nd_backend_zeroizations(void);
int nd_backend_decoded_wipes(void);
void nd_backend_underlay(int present);
int nd_backend_tick(void);
int nd_backend_configured(void);
void nd_backend_replace_underlay(int same_address,int same_index);
int nd_backend_bound(void);
int nd_backend_receive_enabled(void);
int nd_backend_bindings(void);
int nd_backend_sends(void);
int nd_backend_handshake_sent(void);
int nd_backend_direct_output(int device_output);
#ifdef __cplusplus
}
#endif

#endif
