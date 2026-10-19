// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_WIREGUARD_TRANSPORT_HPP
#define NEONDOLL_WIREGUARD_TRANSPORT_HPP
#include <neondoll/network_supervisor.hpp>
#include <array>

namespace neondoll {
struct TunnelIPv6Prefix {
    std::array<uint8_t, 16> address{}; // Network byte order.
    uint8_t length = 128;
};
struct WireGuardEndpoint {
    enum class Family { ipv4, ipv6 };
    Family family = Family::ipv4;
    std::array<uint8_t, 16> address{}; // IPv4 uses first four bytes; remainder zero.
    uint16_t port = 0; // Host byte order. Numeric endpoint only; no DNS in M3.1.
};
struct WireGuardConfig {
    std::array<uint8_t, 32> private_key{}, peer_public_key{}; // Raw keys, not base64.
    WireGuardEndpoint endpoint{};
    std::array<TunnelIPv6Prefix, 2> addresses{};
    std::array<TunnelIPv6Prefix, 4> allowed_ips{};
    size_t address_count = 0, allowed_ip_count = 0;
    ~WireGuardConfig(); // Wipe this copy's private key; never persisted or logged.
    void clear();
    bool valid() const; // Structural checks only, not cryptographic key validation.
};
enum class TunnelState { uninitialized, stopped, waiting_network, starting, up, cleanup_pending };

/** Backend owns all UDP/netif/routes/timers/session material. All operations run
 * on the serialized owner task. start() may partially allocate on failure;
 * stop() must accept partial starts, quiesce callbacks and release ALL resources,
 * erase session/key copies, and be idempotent. Failure retains a cleanup duty.
 * start() must copy needed configuration; it may not retain the config reference.
 * start() must not report up: only status() after authenticated peer activity may.
 * No default routes, private-key logs, or autonomous restarts after stop().
 */
class WireGuardBackend {
public:
    enum class State { down, starting, up, failed };
    virtual ~WireGuardBackend() = default;
    virtual int start(const WireGuardConfig& config) = 0;
    virtual int stop() = 0;
    virtual State status() const = 0;
};
/** Explicit fail-closed production default until the M3.2 backend gate passes.
 * It creates no socket, tunnel or handshake and can never report connectivity. */
class UnavailableWireGuardBackend final : public WireGuardBackend {
public:
    int start(const WireGuardConfig&) override { return -6; }
    int stop() override { return 0; }
    State status() const override { return State::down; }
};
class TunnelTransport {
public:
    virtual ~TunnelTransport() = default;
    virtual int init(const WireGuardConfig& config) = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
    virtual int deinit() = 0;
    virtual int poll() = 0;
    virtual TunnelState status() const = 0;
};
/** ESP-IDF-compatible, allocation-free lifecycle adapter. References outlive it.
 * Call after M1 identity init. In the SAME owner task, call recovery.poll() then
 * tunnel.poll(). Does not poll/control Wi-Fi, take its callback, or use entropy.
 * Wi-Fi loss/change tears down before reacquisition can start a new tunnel.
 * No asynchronous callbacks retain this object. Do not reenter lifecycle calls.
 * Failed start/status requires explicit start() to retry; no tunnel retry policy.
 */
class WireGuardTransport final : public TunnelTransport {
public:
    WireGuardTransport(NetworkConnection& network, WireGuardBackend& backend);
    ~WireGuardTransport() override;
    WireGuardTransport(const WireGuardTransport&) = delete;
    WireGuardTransport& operator=(const WireGuardTransport&) = delete;
    int init(const WireGuardConfig& config) override;
    int start() override; // Arms intent; work deferred to poll(). Idempotent.
    int stop() override; // Cancels intent before cleanup, even if cleanup fails.
    int deinit() override; // Wipes config; retry if backend cleanup fails.
    int poll() override; // At most one backend start OR stop, no sleeps/retry loops.
    TunnelState status() const override { return state_; }
    int last_error() const { return error_; }
private:
    int release();
    NetworkConnection& network_;
    WireGuardBackend& backend_;
    WireGuardConfig config_{};
    TunnelState state_ = TunnelState::uninitialized;
    bool initialized_ = false, desired_ = false, resources_ = false, cleanup_pending_ = false;
    uint64_t network_epoch_ = 0;
    int error_ = 0;
};
} // namespace neondoll
#endif
