// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/wireguard_transport.hpp>
#include <algorithm>

namespace neondoll {
namespace {
template<size_t N> bool nonzero(const std::array<uint8_t, N>& bytes) {
    return std::any_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b != 0; });
}
bool unicast6(const std::array<uint8_t, 16>& ip) {
    // No scoped/link-local addresses: this configuration has no zone identifier.
    if (!nonzero(ip) || ip[0] == 0xff || (ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80)) return false;
    bool first12zero = true;
    for (size_t i = 0; i < 12; ++i) first12zero = first12zero && ip[i] == 0;
    if (first12zero) return false; // Loopback and IPv4-compatible addresses.
    bool mapped = ip[10] == 0xff && ip[11] == 0xff;
    for (size_t i = 0; i < 10; ++i) mapped = mapped && ip[i] == 0;
    return !mapped;
}
bool prefix_valid(const TunnelIPv6Prefix& prefix, bool network) {
    if (!prefix.length || prefix.length > 128 || !unicast6(prefix.address)) return false;
    if (network) { // Require canonical, explicitly scoped routes; never ::/0.
        for (size_t bit = prefix.length; bit < 128; ++bit)
            if (prefix.address[bit / 8] & (0x80u >> (bit % 8))) return false;
    }
    return true;
}
}
WireGuardConfig::~WireGuardConfig() { clear(); }
void WireGuardConfig::clear() {
    volatile uint8_t* key = private_key.data();
    for (size_t i = 0; i < private_key.size(); ++i) key[i] = 0;
    peer_public_key.fill(0); endpoint = {};
    addresses = {}; allowed_ips = {}; address_count = 0; allowed_ip_count = 0;
}
bool WireGuardConfig::valid() const {
    if (!nonzero(private_key) || !nonzero(peer_public_key) || private_key == peer_public_key ||
        !endpoint.port || !address_count || address_count > addresses.size() ||
        !allowed_ip_count || allowed_ip_count > allowed_ips.size()) return false;
    if (endpoint.family == WireGuardEndpoint::Family::ipv4) {
        if (!endpoint.address[0] || endpoint.address[0] >= 224 || endpoint.address[0] == 127 ||
            (endpoint.address[0] == 169 && endpoint.address[1] == 254)) return false;
        for (size_t i = 4; i < endpoint.address.size(); ++i) if (endpoint.address[i]) return false;
    } else if (endpoint.family != WireGuardEndpoint::Family::ipv6 || !unicast6(endpoint.address)) return false;
    for (size_t i = 0; i < address_count; ++i) {
        if (!prefix_valid(addresses[i], false)) return false;
        for (size_t j = 0; j < i; ++j) if (addresses[i].address == addresses[j].address) return false;
    }
    for (size_t i = 0; i < allowed_ip_count; ++i) {
        if (!prefix_valid(allowed_ips[i], true)) return false;
        for (size_t j = 0; j < i; ++j)
            if (allowed_ips[i].address == allowed_ips[j].address && allowed_ips[i].length == allowed_ips[j].length)
                return false;
    }
    return true;
}
WireGuardTransport::WireGuardTransport(NetworkConnection& network, WireGuardBackend& backend)
    : network_(network), backend_(backend) {}
WireGuardTransport::~WireGuardTransport() { deinit(); }
int WireGuardTransport::init(const WireGuardConfig& config) {
    if (initialized_ || resources_ || cleanup_pending_) return -2;
    if (!config.valid()) return -1;
    config_ = config; initialized_ = true; error_ = 0; state_ = TunnelState::stopped;
    return 0;
}
int WireGuardTransport::start() {
    if (cleanup_pending_) return -2;
    if (!initialized_) return -1;
    if (desired_) return 0;
    desired_ = true; error_ = 0; state_ = TunnelState::waiting_network;
    return 0;
}
int WireGuardTransport::release() {
    const auto result = backend_.stop();
    resources_ = cleanup_pending_ = result != 0;
    if (result != 0) { error_ = result; state_ = TunnelState::cleanup_pending; }
    else state_ = !initialized_ ? TunnelState::uninitialized :
        desired_ ? TunnelState::waiting_network : TunnelState::stopped;
    return result;
}
int WireGuardTransport::stop() {
    desired_ = false;
    if (resources_ || cleanup_pending_) return release();
    state_ = initialized_ ? TunnelState::stopped : TunnelState::uninitialized;
    return 0;
}
int WireGuardTransport::deinit() {
    initialized_ = false; config_.clear();
    return stop();
}
int WireGuardTransport::poll() {
    if (cleanup_pending_) return release(); // Bounded retry; never start in same poll.
    if (!desired_) return 0;
    const bool available = network_.running() && network_.available();
    const auto epoch = network_.availability_epoch();
    if (resources_ && (!available || epoch != network_epoch_)) return release();
    if (!available) { state_ = TunnelState::waiting_network; return 0; }
    if (!resources_) {
        network_epoch_ = epoch; resources_ = true;
        const auto result = backend_.start(config_);
        if (result != 0) {
            // Partial allocations must be cleaned before any retry. Cancel intent
            // so failures cannot cause a tight reconnect loop in subsequent polls.
            desired_ = false; cleanup_pending_ = true;
            state_ = TunnelState::cleanup_pending; error_ = result;
            return result;
        }
        state_ = TunnelState::starting; error_ = 0;
        return 0;
    }
    switch (backend_.status()) {
    case WireGuardBackend::State::up: state_ = TunnelState::up; return 0;
    case WireGuardBackend::State::starting: state_ = TunnelState::starting; return 0;
    default:
        desired_ = false; cleanup_pending_ = true;
        state_ = TunnelState::cleanup_pending; error_ = -3;
        return error_;
    }
}
} // namespace neondoll
