// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_NETWORK_SUPERVISOR_HPP
#define NEONDOLL_NETWORK_SUPERVISOR_HPP
#include <neondoll/platform.hpp>

namespace neondoll {
/** Narrow connection control port. Credentials, driver initialization and
 * availability callbacks remain with the application/platform adapter. */
class NetworkConnection {
public:
    virtual ~NetworkConnection() = default;
    virtual int poll() = 0;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int stop() = 0;
    virtual bool available() const = 0;
    virtual bool running() const = 0;
    virtual bool connection_requested() const = 0;
    // Changes on successful IP acquisition/change, even if loss follows within
    // one poll. Compare for inequality; a wrapping counter is sufficient.
    virtual uint64_t availability_epoch() const = 0;
};
struct NetworkRecoveryPolicy {
    uint64_t initial_delay_ms = 1000;
    uint64_t maximum_delay_ms = 30000;
    uint64_t connection_timeout_ms = 30000;
};
/** Serialized owner-task policy. Dependencies must outlive this object. Owns no
 * callbacks, timers or credentials. Call start() after identity init AND adapter
 * station start; then poll() regularly. While supervised, use this object's
 * disconnect/stop/recovery methods, not direct adapter lifecycle calls.
 * Callbacks may inspect getters, but must not reenter methods or destroy it.
 */
class NetworkSupervisor {
public:
    enum class State { stopped, paused, connecting, online, backoff };
    NetworkSupervisor(NetworkConnection& network, Clock& clock, NetworkRecoveryPolicy policy = {});
    ~NetworkSupervisor();
    NetworkSupervisor(const NetworkSupervisor&) = delete;
    NetworkSupervisor& operator=(const NetworkSupervisor&) = delete;
    int start(); // Enable policy; first connect is deferred to poll().
    int request_recovery(); // Coalesced immediate retry from paused/backoff.
    int disconnect(); // Intentional pause; no automatic reconnect.
    int stop(); // Cancel policy first; failure blocks start() until stop() succeeds.
    int poll(); // At most one connect per call. No blocking/timer tasks.
    State state() const { return state_; }
    uint64_t retry_delay_ms() const { return wait_ms_; }
    uint32_t failures() const { return failures_; }
    int last_error() const { return error_; }
private:
    void sample_clock();
    void reset_policy();
    int backoff(int error, bool cancel = true);
    NetworkConnection& network_;
    Clock& clock_;
    NetworkRecoveryPolicy policy_;
    State state_ = State::stopped;
    bool owned_ = false, polling_ = false, manual_ = false, cancel_pending_ = false;
    bool cleanup_pending_ = false;
    uint64_t last_clock_ = 0, elapsed_ = 0, wait_ms_ = 0, next_delay_ = 0, seen_epoch_ = 0;
    uint32_t failures_ = 0;
    int error_ = 0;
};
} // namespace neondoll
#endif
