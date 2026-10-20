// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/doll_link.hpp>
#include <algorithm>

namespace neondoll {
DollLinkConnection::DollLinkConnection(NetworkConnection& network, TunnelTransport& tunnel,
    DollLinkMessageTransport& transport, Clock& clock, DollLinkPolicy policy)
    : network_(network), tunnel_(tunnel), transport_(transport), clock_(clock), policy_(policy) {}
DollLinkConnection::~DollLinkConnection() { disconnect(); }
bool DollLinkConnection::usable() const {
    return network_.running() && network_.available() && tunnel_.status() == TunnelState::up;
}
bool DollLinkConnection::expired(uint64_t now, uint64_t& since, uint64_t duration) {
    if (now < since) since = now; // Clock regression/wrap restarts the bounded interval.
    return now - since >= duration;
}
int DollLinkConnection::connect() {
    if (cleanup_pending_) return link_cleanup_required;
    if (!policy_.connect_timeout_ms || !policy_.retry_delay_ms) return link_invalid;
    if (desired_) return link_ok;
    desired_ = true; retry_ = false; error_ = 0;
    state_ = DollLinkState::waiting_tunnel;
    return link_ok;
}
int DollLinkConnection::release() {
    tx_.clear(); rx_.clear();
    if (resources_) {
        const int result = transport_.close();
        cleanup_pending_ = result != 0;
        if (cleanup_pending_) { error_ = result; state_ = DollLinkState::cleanup_pending; return result; }
        resources_ = false;
    }
    state_ = !desired_ ? DollLinkState::disconnected :
        retry_ ? DollLinkState::backoff : DollLinkState::waiting_tunnel;
    return link_ok;
}
int DollLinkConnection::disconnect() {
    desired_ = retry_ = false;
    return release();
}
int DollLinkConnection::fail(int error, uint64_t now) {
    error_ = error; retry_ = true; since_ = now;
    const int result = release();
    return result ? result : error;
}
int DollLinkConnection::poll() {
    if (cleanup_pending_) return release(); // Never restart in the same cleanup poll.
    if (!desired_) return link_ok;
    const uint64_t now = clock_.now_ms();
    if (resources_ && (!usable() || epoch_ != network_.availability_epoch())) {
        retry_ = false;
        return release();
    }
    if (!usable()) { state_ = DollLinkState::waiting_tunnel; return link_ok; }
    if (!resources_) {
        if (retry_ && !expired(now, since_, policy_.retry_delay_ms)) {
            state_ = DollLinkState::backoff; return link_ok;
        }
        retry_ = false; resources_ = true; epoch_ = network_.availability_epoch();
        since_ = activity_ = now;
        const int result = transport_.begin();
        if (result != link_ok) return fail(result < 0 ? result : link_transport_failed, now);
        state_ = DollLinkState::connecting;
        return link_ok;
    }
    const int result = transport_.poll();
    if (result < 0) return fail(result, now);
    const auto status = transport_.status();
    if (status == DollLinkMessageTransport::State::failed) return fail(link_transport_failed, now);
    if (state_ == DollLinkState::connecting) {
        if (expired(now, since_, policy_.connect_timeout_ms)) return fail(link_timeout, now);
        if (status != DollLinkMessageTransport::State::connected) return link_ok;
        state_ = DollLinkState::connected; activity_ = now; error_ = 0;
    } else if (status != DollLinkMessageTransport::State::connected) {
        return fail(link_transport_failed, now);
    }
    if (policy_.idle_timeout_ms && expired(now, activity_, policy_.idle_timeout_ms))
        return fail(link_timeout, now);
    if (tx_.count) {
        const auto& message = tx_.front();
        const int sent = transport_.send(message.data.data(), message.size);
        if (sent < 0) return fail(sent, now);
        if (sent == link_ok) { tx_.pop(); activity_ = now; }
        else if (sent != link_would_block) return fail(link_transport_failed, now);
    }
    if (rx_.count < queue_capacity) {
        auto& message = rx_.next(); message.size = 0;
        const int received = transport_.receive(message.data.data(), message_capacity, message.size);
        if (received < 0) return fail(received, now);
        if (received == link_ok) {
            if (!message.size || message.size > message_capacity) return fail(link_oversized, now);
            ++rx_.count; activity_ = now;
        } else if (received != link_would_block) return fail(link_transport_failed, now);
    }
    return link_ok;
}
int DollLinkConnection::send(const uint8_t* data, size_t size) {
    if (state_ != DollLinkState::connected || !usable() || epoch_ != network_.availability_epoch())
        return link_unavailable;
    if (!data || !size) return link_invalid;
    if (size > message_capacity) return link_oversized;
    if (tx_.count == queue_capacity) return link_would_block;
    auto& message = tx_.next();
    std::copy_n(data, size, message.data.begin()); message.size = size; ++tx_.count;
    return link_ok;
}
int DollLinkConnection::receive(uint8_t* data, size_t capacity, size_t& size) {
    size = 0;
    if (state_ != DollLinkState::connected || !usable() || epoch_ != network_.availability_epoch())
        return link_unavailable;
    if (!data) return link_invalid;
    if (!rx_.count) return link_would_block;
    const auto& message = rx_.front(); size = message.size;
    if (capacity < size) return link_oversized; // Preserve queued message for retry.
    std::copy_n(message.data.begin(), size, data); rx_.pop();
    return link_ok;
}
} // namespace neondoll
