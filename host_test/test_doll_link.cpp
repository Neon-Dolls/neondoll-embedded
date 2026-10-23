#include <gtest/gtest.h>
#include <neondoll/doll_link.hpp>
#include <algorithm>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace neondoll;
struct Time : Clock {
    uint64_t now = 0;
    int init() override { return 0; }
    int deinit() override { return 0; }
    uint64_t now_ms() override { return now; }
};
struct Network : NetworkConnection {
    bool online = true, active = true;
    uint64_t epoch = 1;
    int poll() override { ADD_FAILURE(); return 0; }
    int connect() override { ADD_FAILURE(); return 0; }
    int disconnect() override { ADD_FAILURE(); return 0; }
    int stop() override { ADD_FAILURE(); return 0; }
    bool available() const override { return online; }
    bool running() const override { return active; }
    bool connection_requested() const override { return true; }
    uint64_t availability_epoch() const override { return epoch; }
};
struct Tunnel : TunnelTransport {
    TunnelState state = TunnelState::up;
    int init(const WireGuardConfig&) override { ADD_FAILURE(); return 0; }
    int start() override { ADD_FAILURE(); return 0; }
    int stop() override { ADD_FAILURE(); return 0; }
    int deinit() override { ADD_FAILURE(); return 0; }
    int poll() override { ADD_FAILURE(); return 0; }
    TunnelState status() const override { return state; }
};
struct Messages : DollLinkMessageTransport {
    State state = State::connecting;
    int begins = 0, closes = 0, live = 0, polls = 0, reads = 0;
    int begin_error = 0, close_error = 0, poll_error = 0, send_result = 0, read_error = 0;
    size_t bogus_size = 0;
    std::deque<std::vector<uint8_t>> incoming;
    std::vector<std::vector<uint8_t>> sent;
    int begin() override {
        EXPECT_EQ(live, 0); ++begins; ++live; state = State::connecting;
        return begin_error;
    }
    int close() override {
        ++closes;
        if (close_error) return close_error;
        live = 0; incoming.clear(); state = State::failed; return 0;
    }
    int poll() override { ++polls; EXPECT_EQ(live, 1); return poll_error; }
    State status() const override { return state; }
    int send(const uint8_t* data, size_t size) override {
        if (!send_result) sent.emplace_back(data, data + size);
        return send_result;
    }
    int receive(uint8_t* data, size_t capacity, size_t& size) override {
        ++reads;
        if (read_error) return read_error;
        if (bogus_size) { size = bogus_size; return 0; }
        if (incoming.empty()) return link_would_block;
        size = incoming.front().size();
        if (size > capacity) return link_oversized;
        std::copy(incoming.front().begin(), incoming.front().end(), data);
        incoming.pop_front(); return 0;
    }
};
class LinkTest : public testing::Test {
protected:
    Time clock; Network network; Tunnel tunnel; Messages messages;
    DollLinkConnection client{network, tunnel, messages, clock, {100, 50, 0}};
    const uint8_t data[3] = {1, 2, 3};
    void online() {
        ASSERT_EQ(client.connect(), 0); ASSERT_EQ(client.poll(), 0);
        messages.state = Messages::State::connected; ASSERT_EQ(client.poll(), 0);
        ASSERT_EQ(client.status(), DollLinkState::connected);
    }
};
TEST_F(LinkTest, RequiresAuthenticatedTunnelAndAvailableRunningNetwork) {
    ASSERT_EQ(client.connect(), 0);
    for (auto state : {TunnelState::uninitialized, TunnelState::stopped, TunnelState::waiting_network,
                       TunnelState::starting, TunnelState::cleanup_pending}) {
        tunnel.state = state; EXPECT_EQ(client.poll(), 0); EXPECT_EQ(messages.begins, 0);
    }
    tunnel.state = TunnelState::up; network.online = false; client.poll();
    network.online = true; network.active = false; client.poll(); EXPECT_EQ(messages.begins, 0);
    network.active = true; client.poll(); EXPECT_EQ(messages.begins, 1);
    EXPECT_EQ(client.status(), DollLinkState::connecting);
}
TEST_F(LinkTest, ConnectDisconnectAreIdempotentAndCancelWaitingIntent) {
    tunnel.state = TunnelState::starting; client.connect(); client.connect();
    client.disconnect(); tunnel.state = TunnelState::up; client.poll(); EXPECT_EQ(messages.begins, 0);
    online(); client.connect(); client.poll(); EXPECT_EQ(messages.begins, 1);
    EXPECT_EQ(client.disconnect(), 0); EXPECT_EQ(client.disconnect(), 0);
    client.poll(); EXPECT_EQ(messages.closes, 1); EXPECT_EQ(messages.live, 0);
    EXPECT_EQ(client.status(), DollLinkState::disconnected);
}
TEST_F(LinkTest, WholeMessageBufferLimitsFifoAndBackpressure) {
    online(); std::vector<uint8_t> large(DollLinkConnection::message_capacity, 7);
    EXPECT_EQ(client.send(nullptr, 1), link_invalid); EXPECT_EQ(client.send(data, 0), link_invalid);
    EXPECT_EQ(client.send(data, large.size() + 1), link_oversized);
    EXPECT_EQ(client.send(large.data(), large.size()), 0); EXPECT_EQ(client.send(data, 3), 0);
    EXPECT_EQ(client.send(data, 3), link_would_block);
    messages.send_result = link_would_block; client.poll(); EXPECT_TRUE(messages.sent.empty());
    messages.send_result = 0; client.poll(); client.poll();
    ASSERT_EQ(messages.sent.size(), 2u); EXPECT_EQ(messages.sent[0], large);
    EXPECT_EQ(messages.sent[1], (std::vector<uint8_t>{1, 2, 3}));
    messages.incoming = {large, {4}, {5}};
    client.poll(); client.poll(); const int reads = messages.reads; client.poll();
    EXPECT_EQ(messages.reads, reads); EXPECT_EQ(messages.incoming.size(), 1u);
    size_t size = 99; uint8_t small[3];
    EXPECT_EQ(client.receive(small, sizeof small, size), link_oversized); EXPECT_EQ(size, large.size());
    std::vector<uint8_t> out(large.size());
    EXPECT_EQ(client.receive(out.data(), out.size(), size), 0); EXPECT_EQ(out, large);
    client.poll(); EXPECT_EQ(client.receive(small, sizeof small, size), 0); EXPECT_EQ(small[0], 4);
    EXPECT_EQ(client.receive(small, sizeof small, size), 0); EXPECT_EQ(small[0], 5);
    EXPECT_EQ(client.receive(small, sizeof small, size), link_would_block); EXPECT_EQ(size, 0u);
}
TEST_F(LinkTest, TunnelLossDiscardsBothQueuesAndReconnectsWithoutReplay) {
    online(); client.send(data, 3); messages.incoming = {{9}};
    messages.send_result = link_would_block; client.poll();
    tunnel.state = TunnelState::starting;
    size_t size; uint8_t out[3];
    EXPECT_EQ(client.send(data, 3), link_unavailable);
    EXPECT_EQ(client.receive(out, sizeof out, size), link_unavailable);
    client.poll(); EXPECT_EQ(messages.live, 0);
    tunnel.state = TunnelState::up; client.poll();
    messages.state = Messages::State::connected; messages.send_result = 0; client.poll();
    EXPECT_EQ(client.receive(out, sizeof out, size), link_would_block); EXPECT_TRUE(messages.sent.empty());
    EXPECT_EQ(messages.begins, 2);
}
TEST_F(LinkTest, NetworkEpochInvalidatesEvenWhenTunnelStillReportsUp) {
    online(); ++network.epoch;
    EXPECT_EQ(client.send(data, 3), link_unavailable);
    client.poll(); EXPECT_EQ(messages.live, 0); client.poll(); EXPECT_EQ(messages.begins, 2);
    network.online = false; client.poll(); EXPECT_EQ(messages.live, 0);
}
TEST_F(LinkTest, FailedPartialBeginCleansBeforeBoundedRetry) {
    messages.begin_error = -22; client.connect(); EXPECT_EQ(client.poll(), -22);
    EXPECT_EQ(messages.live, 0); EXPECT_EQ(client.status(), DollLinkState::backoff);
    for (int i = 0; i < 10; ++i) client.poll(); EXPECT_EQ(messages.begins, 1);
    clock.now = 49; client.poll(); EXPECT_EQ(messages.begins, 1);
    clock.now = 50; messages.begin_error = 0; client.poll(); EXPECT_EQ(messages.begins, 2);
    messages.state = Messages::State::connected; client.poll(); EXPECT_EQ(client.last_error(), 0);
}
TEST_F(LinkTest, FailedDisconnectBlocksRestartUntilSuccessfulCleanup) {
    online(); messages.close_error = -23; EXPECT_EQ(client.disconnect(), -23);
    EXPECT_EQ(client.status(), DollLinkState::cleanup_pending);
    EXPECT_EQ(client.connect(), link_cleanup_required); client.poll();
    EXPECT_EQ(messages.begins, 1); EXPECT_EQ(messages.live, 1);
    messages.close_error = 0; client.poll(); EXPECT_EQ(messages.live, 0);
    clock.now = 99999; client.poll(); EXPECT_EQ(messages.begins, 1);
    online(); EXPECT_EQ(messages.begins, 2);
}
TEST_F(LinkTest, FailedBeginAndFailedCleanupCannotAllocateDuplicateResources) {
    client.connect(); messages.begin_error = -10; messages.close_error = -11;
    EXPECT_EQ(client.poll(), -11); clock.now = 1000;
    for (int i = 0; i < 5; ++i) EXPECT_EQ(client.poll(), -11);
    EXPECT_EQ(messages.begins, 1); messages.close_error = 0; EXPECT_EQ(client.poll(), 0);
    EXPECT_EQ(messages.begins, 1); messages.begin_error = 0; client.poll(); EXPECT_EQ(messages.begins, 2);
}
TEST_F(LinkTest, ConnectTimeoutBackoffAndShutdownDuringBackoff) {
    client.connect(); client.poll(); clock.now = 99; client.poll(); EXPECT_EQ(messages.live, 1);
    clock.now = 100; EXPECT_EQ(client.poll(), link_timeout); EXPECT_EQ(messages.live, 0);
    client.disconnect(); clock.now = 10000; client.poll(); EXPECT_EQ(messages.begins, 1);
    EXPECT_EQ(client.status(), DollLinkState::disconnected);
}
TEST_F(LinkTest, TransportFailuresCleanAndRecoverWithoutQueuedReplay) {
    online(); client.send(data, 3); messages.poll_error = -30;
    EXPECT_EQ(client.poll(), -30); EXPECT_EQ(messages.live, 0);
    clock.now = 50; messages.poll_error = 0; client.poll();
    messages.state = Messages::State::connected; client.poll(); EXPECT_TRUE(messages.sent.empty());
    messages.state = Messages::State::failed; EXPECT_EQ(client.poll(), link_transport_failed);
}
TEST_F(LinkTest, SendAndReceiveFailuresCloseConnection) {
    online(); client.send(data, 3); messages.send_result = -31;
    EXPECT_EQ(client.poll(), -31); EXPECT_EQ(messages.live, 0);
    clock.now = 50; messages.send_result = 0; client.poll(); messages.state = Messages::State::connected;
    messages.read_error = link_oversized; EXPECT_EQ(client.poll(), link_oversized); EXPECT_EQ(messages.live, 0);
}
TEST_F(LinkTest, OversizedOrEmptyAdapterMessageFailsClosed) {
    online(); messages.bogus_size = DollLinkConnection::message_capacity + 1;
    EXPECT_EQ(client.poll(), link_oversized); EXPECT_EQ(messages.live, 0);
    clock.now = 50; messages.bogus_size = 0; client.poll(); messages.state = Messages::State::connected;
    messages.incoming = {{}}; EXPECT_EQ(client.poll(), link_oversized);
}
TEST_F(LinkTest, IdleTimeoutUsesClockWithoutHeartbeatOrBlocking) {
    client.disconnect();
    DollLinkConnection idle{network, tunnel, messages, clock, {100, 50, 20}};
    idle.connect(); idle.poll(); messages.state = Messages::State::connected; idle.poll();
    clock.now = 19; idle.send(data, 3); idle.poll();
    clock.now = 38; EXPECT_EQ(idle.poll(), 0);
    clock.now = 39; EXPECT_EQ(idle.poll(), link_timeout); EXPECT_EQ(messages.live, 0);
}
TEST_F(LinkTest, ClockRegressionAndNearMaximumDoNotTriggerRetryStorm) {
    clock.now = std::numeric_limits<uint64_t>::max() - 20;
    client.connect(); client.poll(); clock.now = 1; EXPECT_EQ(client.poll(), 0);
    EXPECT_EQ(messages.live, 1); clock.now = 100; EXPECT_EQ(client.poll(), 0);
    clock.now = 101; EXPECT_EQ(client.poll(), link_timeout);
    clock.now = 0; client.poll(); clock.now = 49; client.poll(); EXPECT_EQ(messages.begins, 1);
    clock.now = 50; client.poll(); EXPECT_EQ(messages.begins, 2);
}
TEST_F(LinkTest, RepeatedRecoveryAndDestructionReleaseAllResources) {
    for (int i = 0; i < 100; ++i) {
        online(); tunnel.state = TunnelState::stopped; client.poll();
        tunnel.state = TunnelState::up; client.disconnect(); EXPECT_EQ(messages.live, 0);
    }
    {
        DollLinkConnection scoped{network, tunnel, messages, clock};
        scoped.connect(); scoped.poll(); EXPECT_EQ(messages.live, 1);
    }
    EXPECT_EQ(messages.live, 0);
}
TEST_F(LinkTest, RejectsZeroTimeoutOrRetryIntervalAndDoesNotAllocate) {
    for (auto policy : {DollLinkPolicy{0, 50, 0}, DollLinkPolicy{100, 0, 0}}) {
        DollLinkConnection invalid{network, tunnel, messages, clock, policy};
        EXPECT_EQ(invalid.connect(), link_invalid); invalid.poll();
    }
    EXPECT_EQ(messages.begins, 0);
}
} // namespace
