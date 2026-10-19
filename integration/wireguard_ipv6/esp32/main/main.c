// Compile/link probe only; no production keys or network initialization.
#include "esp_wireguard.h"
#include "lwip/opt.h"
#include <stdio.h>
#if !LWIP_IPV6
#error "IPv6 is required"
#endif
esp_err_t (*volatile probe_init)(wireguard_config_t*, wireguard_ctx_t*) = esp_wireguard_init;
esp_err_t (*volatile probe_connect)(wireguard_ctx_t*) = esp_wireguard_connect;
esp_err_t (*volatile probe_stop)(wireguard_ctx_t*) = esp_wireguard_disconnect;
esp_err_t (*volatile probe_status)(const wireguard_ctx_t*) = esp_wireguard_peer_is_up;
void app_main(void) {
    if (probe_init && probe_connect && probe_stop && probe_status)
        puts("M3.2a compile probe only; no tunnel started; M3.2 incomplete.");
}
