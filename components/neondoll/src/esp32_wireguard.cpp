// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/esp32_wireguard.hpp>
#include <cstring>
extern "C" {
#include <esp_netif.h>
#include <lwip/tcpip.h>
#include <lwip/ip.h>
#include <lwip/udp.h>
#include <mbedtls/base64.h>
#include <mbedtls/platform_util.h>
#include <wireguardif.h>
#include <wireguard.h>
}
#if !LWIP_TCPIP_CORE_LOCKING || !LWIP_IPV6 || !LWIP_IPV6_ADDRESS_LIFETIMES || !LWIP_NETIF_EXT_STATUS_CALLBACK
#error "WireGuard requires TCP/IP core locking, IPv6 address lifetimes and netif removal callbacks"
#endif
static_assert(WIREGUARD_MAX_SRC_IPS >= 4, "Build vendor and consumer with four AllowedIPs slots");
static_assert(LWIP_IPV6_NUM_ADDRESSES >= 2, "Two tunnel addresses require two lwIP address slots");
namespace neondoll {
namespace {
struct CoreLock {
    CoreLock() { LOCK_TCPIP_CORE(); }
    ~CoreLock() { UNLOCK_TCPIP_CORE(); }
};
struct Temporary {
    char private_key[45]{}, public_key[45]{};
    wireguardif_init_data init{};
    wireguardif_peer peer{};
    ~Temporary() { mbedtls_platform_zeroize(this, sizeof(*this)); }
};
bool station_name(char (&name)[8]) {
    auto* station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    return station && esp_netif_get_netif_impl_name(station, name) == ESP_OK;
}
ip_addr_t ipv6(const std::array<uint8_t, 16>& bytes) {
    ip_addr_t ip{}; ip_addr_set_zero_ip6(&ip);
    std::memcpy(ip_2_ip6(&ip)->addr, bytes.data(), bytes.size());
    return ip;
}
ip_addr_t mask(uint8_t length) {
    std::array<uint8_t, 16> bytes{};
    for (unsigned i = 0; i < length; ++i) bytes[i / 8] |= 0x80u >> (i % 8);
    return ipv6(bytes);
}
bool authenticated(const wireguard_keypair& key, uint32_t now) {
    return key.valid && key.receiving_valid && key.last_rx != 0 &&
        uint32_t(now - key.last_rx) < REJECT_AFTER_TIME * 1000u &&
        uint32_t(now - key.keypair_millis) < REJECT_AFTER_TIME * 1000u;
}
}
struct Esp32WireGuardBackend::Impl {
    netif interface{};
    bool used = false, added = false;
    uint8_t peer_index = WIREGUARDIF_INVALID_INDEX;
    netif_ext_callback_t removal_callback{};
    static void underlay_changed(netif* removed, netif_nsc_reason_t reason,
                                 const netif_ext_callback_args_t*) {
        if (!(reason & LWIP_NSC_NETIF_REMOVED) || !slot_.added) return;
        auto* device = static_cast<wireguard_device*>(slot_.interface.state);
        if (device && device->underlying_netif == removed) {
            // Removal is synchronous under the core lock, before the netif can
            // be freed/reused. Latch invalid even if BOTH pointer/index recur.
            device->underlying_netif = nullptr;
            udp_recv(device->udp_pcb, nullptr, nullptr);
        }
    }
};
Esp32WireGuardBackend::Impl Esp32WireGuardBackend::slot_{};
Esp32WireGuardBackend::~Esp32WireGuardBackend() { stop(); }
void Esp32WireGuardBackend::release_locked() {
    if (impl_->added) {
        // Serialized against timers/UDP input. Unregister their references BEFORE
        // removing/wiping the interface; vendor fini wipes the entire device.
        netif_remove_ext_callback(&impl_->removal_callback);
        wireguardif_shutdown(&impl_->interface);
        netif_remove(&impl_->interface);
        wireguardif_fini(&impl_->interface);
    }
    wireguard_platform_deinit();
    mbedtls_platform_zeroize(impl_, sizeof(*impl_));
    impl_ = nullptr;
}
int Esp32WireGuardBackend::start(const WireGuardConfig& config) {
    if (impl_) return -2; // No implicit reconfiguration of a live peer.
    if (!config.valid()) return -1;
    // Safe before TCP/IP exists: never lock an uninitialized lwIP mutex.
    char name[8]{};
    // ESP-NETIF name lookup dispatches IPC: resolve BEFORE taking the core lock.
    if (!station_name(name)) return -1;
    CoreLock lock;
    if (slot_.used) return -2;
    auto* underlay = netif_find(name);
    if (!underlay || !netif_is_up(underlay) || !netif_is_link_up(underlay)) return -1;
    if (config.endpoint.family == WireGuardEndpoint::Family::ipv4 &&
        ip4_addr_isany(netif_ip4_addr(underlay))) return -1;
    impl_ = &slot_; impl_->used = true;
    Temporary temporary; // All key encodings and pointer-bearing init data wiped on every exit.
    auto fail = [this](int error) { error_ = error; release_locked(); return -3; };
    const int platform = wireguard_platform_init();
    if (platform != ESP_OK) return fail(platform);
    size_t length = 0;
    int result = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(temporary.private_key),
        sizeof(temporary.private_key), &length, config.private_key.data(), config.private_key.size());
    if (result != 0) return fail(result);
    result = mbedtls_base64_encode(reinterpret_cast<unsigned char*>(temporary.public_key),
        sizeof(temporary.public_key), &length, config.peer_public_key.data(), config.peer_public_key.size());
    if (result != 0) return fail(result);
    temporary.init.private_key = temporary.private_key;
    temporary.init.bind_netif = underlay;
    ip4_addr_t zero{};
    if (!netif_add(&impl_->interface, &zero, &zero, &zero, &temporary.init, wireguardif_init, ip_input))
        return fail(ERR_IF); // Vendor init releases any partial UDP/device allocations.
    impl_->added = true;
    netif_add_ext_callback(&impl_->removal_callback, Impl::underlay_changed);
    for (size_t i = 0; i < config.address_count; ++i) {
        const auto address = ipv6(config.addresses[i].address);
        netif_ip6_addr_set(&impl_->interface, i, ip_2_ip6(&address));
        netif_ip6_addr_set_valid_life(&impl_->interface, i, IP6_ADDR_LIFE_INFINITE);
        netif_ip6_addr_set_pref_life(&impl_->interface, i, IP6_ADDR_LIFE_INFINITE);
        netif_ip6_addr_set_state(&impl_->interface, i, IP6_ADDR_PREFERRED);
    }
    netif_set_ip6_autoconfig_enabled(&impl_->interface, 0);
    wireguardif_peer_init(&temporary.peer);
    temporary.peer.public_key = temporary.public_key;
    temporary.peer.allowed_ip = ipv6(config.allowed_ips[0].address);
    temporary.peer.allowed_mask = mask(config.allowed_ips[0].length);
    if (config.endpoint.family == WireGuardEndpoint::Family::ipv4) {
        ip_addr_set_zero_ip4(&temporary.peer.endpoint_ip);
        std::memcpy(&ip_2_ip4(&temporary.peer.endpoint_ip)->addr, config.endpoint.address.data(), 4);
    } else temporary.peer.endpoint_ip = ipv6(config.endpoint.address);
    temporary.peer.endport_port = config.endpoint.port;
    result = wireguardif_add_peer(&impl_->interface, &temporary.peer, &impl_->peer_index);
    if (result != ERR_OK) return fail(result);
    for (size_t i = 1; i < config.allowed_ip_count; ++i) {
        result = wireguardif_add_allowed_ip(&impl_->interface, impl_->peer_index,
            ipv6(config.allowed_ips[i].address), mask(config.allowed_ips[i].length));
        if (result != ERR_OK) return fail(result);
    }
    netif_set_up(&impl_->interface);
    result = wireguardif_connect(&impl_->interface, impl_->peer_index);
    if (result != ERR_OK) return fail(result);
    error_ = 0;
    return 0; // Only association/handshake intent; status still requires authenticated RX.
}
int Esp32WireGuardBackend::stop() {
    if (!impl_) return 0;
    CoreLock lock;
    release_locked(); error_ = 0;
    return 0;
}
WireGuardBackend::State Esp32WireGuardBackend::status() const {
    if (!impl_) return State::down;
    char name[8]{};
    if (!station_name(name)) return State::starting;
    CoreLock lock;
    auto* device = static_cast<wireguard_device*>(impl_->interface.state);
    if (!device || !device->valid || impl_->peer_index >= WIREGUARD_MAX_PEERS) return State::failed;
    // Resolve the station afresh; M2 may have destroyed the old netif already.
    auto* underlay = netif_find(name);
    if (!underlay || underlay != device->underlying_netif || !netif_is_up(underlay) || !netif_is_link_up(underlay))
        return State::starting; // M3.1 poll owns loss teardown/recovery; never resurrect Wi-Fi here.
    const auto& peer = device->peers[impl_->peer_index];
    const auto now = wireguard_sys_now();
    return peer.valid && (authenticated(peer.curr_keypair, now) || authenticated(peer.prev_keypair, now)) ?
        State::up : State::starting;
}
}
