// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/network_supervisor.hpp>
#include <limits>

namespace neondoll {
NetworkSupervisor::NetworkSupervisor(NetworkConnection& network, Clock& clock, NetworkRecoveryPolicy policy)
    : network_(network), clock_(clock), policy_(policy) {}
NetworkSupervisor::~NetworkSupervisor() { stop(); }
void NetworkSupervisor::reset_policy() {
    failures_ = 0; elapsed_ = 0; wait_ms_ = 0;
    next_delay_ = policy_.initial_delay_ms; error_ = 0;
}
void NetworkSupervisor::sample_clock() {
    const auto now = clock_.now_ms();
    // A backward jump/wrap contributes no elapsed time. Rebase so subsequent
    // progression still works; never turn a clock reset into a huge overdue retry.
    const auto delta = now >= last_clock_ ? now - last_clock_ : 0;
    const auto maximum = std::numeric_limits<uint64_t>::max();
    elapsed_ = delta > maximum - elapsed_ ? maximum : elapsed_ + delta;
    last_clock_ = now;
}
int NetworkSupervisor::start() {
    if (polling_ || state_ != State::stopped || cleanup_pending_) return -2;
    if (!policy_.initial_delay_ms || policy_.maximum_delay_ms < policy_.initial_delay_ms ||
        !policy_.connection_timeout_ms || !network_.running()) return -1;
    reset_policy();
    last_clock_ = clock_.now_ms(); seen_epoch_ = network_.availability_epoch();
    manual_ = false; cancel_pending_ = false;
    owned_ = true; state_ = State::backoff; // Initial delay is zero, first attempt in poll.
    return 0;
}
int NetworkSupervisor::request_recovery() {
    if (polling_) return -2;
    if (state_ == State::stopped) return -1;
    if (state_ == State::paused || state_ == State::backoff) manual_ = true;
    return 0;
}
int NetworkSupervisor::disconnect() {
    if (polling_) return -2;
    if (state_ == State::stopped) return -1;
    state_ = State::paused; manual_ = false; reset_policy();
    const auto result = network_.disconnect();
    cancel_pending_ = result != 0; error_ = result;
    return result;
}
int NetworkSupervisor::stop() {
    if (polling_) return -2;
    state_ = State::stopped; manual_ = false; reset_policy();
    cancel_pending_ = false;
    if (!owned_) return 0;
    const auto result = network_.stop();
    owned_ = result != 0; // Failed teardown can be retried, never a reason to reconnect.
    cleanup_pending_ = result != 0; // running() alone cannot prove teardown completed.
    error_ = result;
    return result;
}
int NetworkSupervisor::backoff(int error, bool cancel) {
    // Cancel pending association/DHCP/internal retries once, without a retry loop.
    const auto result = cancel ? network_.disconnect() : error;
    cancel_pending_ = result != 0;
    error_ = result != 0 ? result : error;
    state_ = State::backoff; elapsed_ = 0; wait_ms_ = next_delay_;
    next_delay_ = next_delay_ > policy_.maximum_delay_ms / 2 ?
        policy_.maximum_delay_ms : next_delay_ * 2;
    if (failures_ != std::numeric_limits<uint32_t>::max()) ++failures_;
    return error_;
}
int NetworkSupervisor::poll() {
    if (polling_) return -2;
    polling_ = true;
    struct Guard { bool& flag; ~Guard() { flag = false; } } guard{polling_};
    const auto result = network_.poll(); // Preserve adapter callback timing and ordering.
    const auto epoch = network_.availability_epoch();
    const bool acquired = epoch != seen_epoch_;
    seen_epoch_ = epoch;
    if (state_ == State::stopped || (state_ == State::paused && !manual_)) return 0;
    sample_clock();
    if (!network_.running()) {
        // A station stop is not an ordinary Wi-Fi outage. Leave driver restart
        // to the application, and never resurrect an explicitly stopped station.
        state_ = State::stopped; manual_ = false; error_ = -1;
        return -1;
    }
    if (acquired) {
        // IP may have been acquired and lost within the same bounded poll.
        reset_policy(); state_ = State::online; manual_ = false; cancel_pending_ = false;
    }
    if (network_.available()) {
        state_ = State::online; manual_ = false; cancel_pending_ = false; reset_policy();
        return 0;
    }
    if (manual_) {
        manual_ = false; state_ = State::backoff; elapsed_ = 0; wait_ms_ = 0;
    }
    if (state_ == State::online) return backoff(result != 0 ? result : -3);
    if (state_ == State::connecting) {
        if (!network_.connection_requested()) return backoff(result != 0 ? result : -3);
        if (elapsed_ >= policy_.connection_timeout_ms) return backoff(-5);
        return 0;
    }
    if (state_ != State::backoff || elapsed_ < wait_ms_) return 0;
    if (cancel_pending_) {
        const auto cancel = network_.disconnect();
        if (cancel != 0) return backoff(cancel, false);
        cancel_pending_ = false;
    }
    const auto connect = network_.connect(); // Exactly one attempt, even after a large clock jump.
    if (connect != 0) return backoff(connect);
    state_ = State::connecting; elapsed_ = 0;
    return 0;
}
} // namespace neondoll
