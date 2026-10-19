// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
// Compile/link probe only. Deliberately creates no interface or peer, uses no keys.
#include "esp_wireguard.h"
#include "lwip/opt.h"
#include <stdio.h>

#if !LWIP_IPV6
#error "The feasibility build must enable IPv6; an IPv4-only build proves nothing."
#endif

// Keep the relevant real implementation reachable at link time without running
// a rejected backend or shipping credentials in a flash image.
esp_err_t (*volatile probe_init)(wireguard_config_t*, wireguard_ctx_t*) = esp_wireguard_init;
esp_err_t (*volatile probe_connect)(wireguard_ctx_t*) = esp_wireguard_connect;
esp_err_t (*volatile probe_stop)(wireguard_ctx_t*) = esp_wireguard_disconnect;
esp_err_t (*volatile probe_status)(const wireguard_ctx_t*) = esp_wireguard_peer_is_up;

void app_main(void) {
    if (probe_init && probe_connect && probe_stop && probe_status) {
        puts("WireGuard feasibility build only: IPv6 backend gate BLOCKED; no tunnel started.");
    }
}
