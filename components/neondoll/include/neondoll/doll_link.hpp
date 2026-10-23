// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_DOLL_LINK_HPP
#define NEONDOLL_DOLL_LINK_HPP
#include <neondoll/wireguard_transport.hpp>

namespace neondoll {
enum class DollLinkState { disconnected, waiting_tunnel, connecting, connected, backoff, cleanup_pending };
// Return codes shared by client and message transport. Other negative values
// from an adapter are preserved in last_error(). No authentication-ready state.
enum DollLinkResult { link_ok = 0, link_would_block = 1, link_invalid = -1,
    link_cleanup_required = -2, link_unavailable = -3, link_timeout = -4,
    link_transport_failed = -5, link_oversized = -6 };
struct DollLinkPolicy {
    uint64_t connect_timeout_ms = 10000;
    uint64_t retry_delay_ms = 1000; // Fixed bounded retry interval, not a busy loop.
    uint64_t idle_timeout_ms = 0; // Disabled by default; no invented heartbeat.
};
/** Injected message-oriented transport, NOT a new wire protocol. A future
 * WebSocket adapter owns endpoint/trust/auth configuration and wire framing.
 * All calls are nonblocking on the serialized owner task. begin() may allocate
 * partially; close() accepts partial begins and is idempotent. Even on failure,
 * close() quiesces callbacks; a failure retains resources requiring another close.
 * No callbacks may retain the client. Adapter/dependencies outlive the client.
 * send(): 0 accepts the WHOLE message, 1 accepts nothing, <0 fails.
 * receive(): 0 returns ONE whole nonempty message, 1 means none; oversized input
 * returns link_oversized without writing past capacity (never silently truncate).
 * Message boundaries are supplied by the adapter, never inferred from TCP reads.
 * poll() advances at most bounded work, status() is connection status ONLY.
 */
class DollLinkMessageTransport {
public:
    enum class State { connecting, connected, failed };
    virtual ~DollLinkMessageTransport() = default;
    virtual int begin() = 0;
    virtual int close() = 0;
    virtual int poll() = 0;
    virtual State status() const = 0;
    virtual int send(const uint8_t* data, size_t size) = 0;
    virtual int receive(uint8_t* data, size_t capacity, size_t& size) = 0;
};
class DollLinkClient {
public:
    virtual ~DollLinkClient() = default;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
    virtual int poll() = 0;
    virtual int send(const uint8_t* data, size_t size) = 0;
    virtual int receive(uint8_t* data, size_t capacity, size_t& size) = 0;
    virtual DollLinkState status() const = 0;
};
/** Transport-neutral M4.1 foundation; connected does NOT mean authenticated
 * Body, negotiated Body Protocol, or ready for capability execution.
 * Call after M1 init. Owner task order: network supervisor, tunnel, this client.
 * Client never starts/stops dependencies or consumes network callbacks/entropy.
 * Poll after every tunnel transition, before restarting it: TunnelTransport has
 * no generation counter. Network epochs additionally invalidate stale sessions.
 * connect arms intent; disconnect cancels before cleanup, including failed close.
 * Failures/loss clear queued messages; no replay across connection generations.
 */
class DollLinkConnection final : public DollLinkClient {
public:
    static constexpr size_t message_capacity = 1024, queue_capacity = 2;
    DollLinkConnection(NetworkConnection& network, TunnelTransport& tunnel,
                       DollLinkMessageTransport& transport, Clock& clock, DollLinkPolicy policy = {});
    ~DollLinkConnection() override;
    DollLinkConnection(const DollLinkConnection&) = delete;
    DollLinkConnection& operator=(const DollLinkConnection&) = delete;
    int connect() override;
    int disconnect() override;
    int poll() override;
    int send(const uint8_t* data, size_t size) override;
    int receive(uint8_t* data, size_t capacity, size_t& size) override;
    DollLinkState status() const override { return state_; }
    int last_error() const { return error_; }
private:
    struct Message { std::array<uint8_t, message_capacity> data{}; size_t size = 0; };
    struct Queue {
        std::array<Message, queue_capacity> messages{};
        size_t head = 0, count = 0;
        Message& front() { return messages[head]; }
        Message& next() { return messages[(head + count) % queue_capacity]; }
        void pop() { front() = {}; head = (head + 1) % queue_capacity; --count; }
        void clear() { messages = {}; head = count = 0; }
    };
    bool usable() const;
    bool expired(uint64_t now, uint64_t& since, uint64_t duration);
    int release();
    int fail(int error, uint64_t now);
    NetworkConnection& network_;
    TunnelTransport& tunnel_;
    DollLinkMessageTransport& transport_;
    Clock& clock_;
    DollLinkPolicy policy_;
    Queue tx_{}, rx_{};
    DollLinkState state_ = DollLinkState::disconnected;
    bool desired_ = false, resources_ = false, cleanup_pending_ = false, retry_ = false;
    uint64_t epoch_ = 0, since_ = 0, activity_ = 0;
    int error_ = 0;
};
} // namespace neondoll
#endif
