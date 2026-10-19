#include <gtest/gtest.h>
#include <neondoll/wireguard_transport.hpp>

namespace {
using namespace neondoll;
struct Network : NetworkConnection {
    bool online = false, active = true;
    uint64_t epoch = 0;
    int connects = 0, polls = 0;
    int poll() override { ++polls; return 0; }
    int connect() override { ++connects; return 0; }
    int disconnect() override { online = false; return 0; }
    int stop() override { active = false; online = false; return 0; }
    bool available() const override { return online; }
    bool running() const override { return active; }
    bool connection_requested() const override { return false; }
    uint64_t availability_epoch() const override { return epoch; }
    void acquire() { online = true; ++epoch; }
};
struct Backend : WireGuardBackend {
    int starts = 0, stops = 0, live = 0, start_error = 0, stop_error = 0;
    State state = State::down;
    WireGuardConfig received;
    int start(const WireGuardConfig& config) override {
        ++starts; ++live; received = config; // Includes partial allocation on error.
        state = State::starting;
        return start_error;
    }
    int stop() override {
        ++stops;
        if (stop_error) return stop_error;
        live = 0; state = State::down; received.clear(); return 0;
    }
    State status() const override { return state; }
};
WireGuardConfig configuration() {
    WireGuardConfig c;
    c.private_key.fill(0x11); c.peer_public_key.fill(0x22);
    c.endpoint.address[0] = 192; c.endpoint.address[1] = 0;
    c.endpoint.address[2] = 2; c.endpoint.address[3] = 1; c.endpoint.port = 51820;
    c.address_count = c.allowed_ip_count = 1;
    c.addresses[0].address[0] = 0xfd; c.addresses[0].address[15] = 2;
    c.allowed_ips[0].address[0] = 0xfd; c.allowed_ips[0].length = 64;
    return c;
}
class TunnelTest : public testing::Test {
protected:
    Network network; Backend backend; WireGuardTransport tunnel{network, backend};
    WireGuardConfig config = configuration();
    void initialize() { ASSERT_EQ(tunnel.init(config), 0); }
    void connect() {
        initialize(); network.acquire(); ASSERT_EQ(tunnel.start(), 0);
        ASSERT_EQ(tunnel.poll(), 0); ASSERT_EQ(backend.live, 1);
    }
};
TEST(WireGuardConfiguration, ValidIPv4AndIPv6EndpointAndMultipleOverlayAddresses) {
    auto c = configuration(); EXPECT_TRUE(c.valid());
    c.endpoint.family = WireGuardEndpoint::Family::ipv6;
    c.endpoint.address = {}; c.endpoint.address[0] = 0x20; c.endpoint.address[1] = 1;
    c.endpoint.address[15] = 1;
    c.address_count = 2; c.addresses[1] = c.addresses[0]; c.addresses[1].address[15] = 3;
    c.allowed_ip_count = 2; c.allowed_ips[1] = c.allowed_ips[0]; c.allowed_ips[1].address[7] = 1;
    EXPECT_TRUE(c.valid());
}
TEST(WireGuardConfiguration, RejectsMissingOrIdenticalKeysAndEndpointPort) {
    auto c = configuration(); c.private_key.fill(0); EXPECT_FALSE(c.valid());
    c = configuration(); c.peer_public_key.fill(0); EXPECT_FALSE(c.valid());
    c = configuration(); c.peer_public_key = c.private_key; EXPECT_FALSE(c.valid());
    c = configuration(); c.endpoint.port = 0; EXPECT_FALSE(c.valid());
}
TEST(WireGuardConfiguration, RejectsInvalidEndpointsAndUnsupportedScope) {
    for (uint8_t first : {0, 127, 224, 255}) {
        auto c = configuration(); c.endpoint.address[0] = first; EXPECT_FALSE(c.valid());
    }
    auto c = configuration(); c.endpoint.address[5] = 1; EXPECT_FALSE(c.valid());
    c = configuration(); c.endpoint.address[0] = 169; c.endpoint.address[1] = 254; EXPECT_FALSE(c.valid());
    for (uint8_t first : {0, 0xff, 0xfe}) {
        c = configuration(); c.endpoint.family = WireGuardEndpoint::Family::ipv6;
        c.endpoint.address = {}; c.endpoint.address[0] = first; c.endpoint.address[1] = 0x80;
        if (!first) c.endpoint.address[1] = 0;
        EXPECT_FALSE(c.valid());
    }
    c = configuration(); c.endpoint.family = static_cast<WireGuardEndpoint::Family>(99); EXPECT_FALSE(c.valid());
}
TEST(WireGuardConfiguration, RejectsUnboundedCountsDefaultRoutesAndMalformedPrefixes) {
    for (size_t count : {size_t{0}, size_t{3}, size_t{999}}) {
        auto c = configuration(); c.address_count = count; EXPECT_FALSE(c.valid());
    }
    for (size_t count : {size_t{0}, size_t{5}, size_t{999}}) {
        auto c = configuration(); c.allowed_ip_count = count; EXPECT_FALSE(c.valid());
    }
    for (uint8_t length : {0, 129, 255}) {
        auto c = configuration(); c.allowed_ips[0].length = length; EXPECT_FALSE(c.valid());
        c = configuration(); c.addresses[0].length = length; EXPECT_FALSE(c.valid());
    }
    auto c = configuration(); c.allowed_ips[0].address = {}; c.allowed_ips[0].length = 0; EXPECT_FALSE(c.valid());
    c = configuration(); c.allowed_ips[0].address[15] = 1; EXPECT_FALSE(c.valid());
    c = configuration(); c.addresses[0].address[0] = 0xff; EXPECT_FALSE(c.valid());
    c = configuration(); c.addresses[0].address = {}; c.addresses[0].address[15] = 1; EXPECT_FALSE(c.valid());
    c = configuration(); c.addresses[0].address = {}; c.addresses[0].address[10] = 0xff;
    c.addresses[0].address[11] = 0xff; c.addresses[0].address[12] = 192; EXPECT_FALSE(c.valid());
}
TEST(WireGuardConfiguration, RejectsDuplicatesAndChecksNonByteAlignedRouteMasks) {
    auto c = configuration(); c.address_count = 2; c.addresses[1] = c.addresses[0]; EXPECT_FALSE(c.valid());
    c = configuration(); c.allowed_ip_count = 2; c.allowed_ips[1] = c.allowed_ips[0]; EXPECT_FALSE(c.valid());
    c = configuration(); c.allowed_ips[0].length = 65; c.allowed_ips[0].address[8] = 0x80;
    EXPECT_TRUE(c.valid()); c.allowed_ips[0].address[8] = 0x81; EXPECT_FALSE(c.valid());
}
TEST_F(TunnelTest, InvalidAndRepeatedInitializationDoNotTouchBackend) {
    EXPECT_EQ(tunnel.start(), -1); auto bad = config; bad.private_key.fill(0);
    EXPECT_EQ(tunnel.init(bad), -1); EXPECT_EQ(tunnel.status(), TunnelState::uninitialized);
    initialize(); EXPECT_EQ(tunnel.init(config), -2);
    EXPECT_EQ(backend.starts, 0); EXPECT_EQ(backend.stops, 0);
}
TEST_F(TunnelTest, OfflineStartWaitsAndBackendStartDoesNotClaimConnected) {
    initialize(); ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.start(), 0);
    for (int i = 0; i < 100; ++i) EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 0); EXPECT_EQ(tunnel.status(), TunnelState::waiting_network);
    network.acquire(); ASSERT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(tunnel.status(), TunnelState::starting); EXPECT_EQ(backend.starts, 1);
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(tunnel.status(), TunnelState::starting);
    backend.state = WireGuardBackend::State::up;
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(tunnel.status(), TunnelState::up);
    EXPECT_EQ(network.polls, 0); EXPECT_EQ(network.connects, 0);
}
TEST_F(TunnelTest, CopiesConfigurationIntoRamAndPassesExactValues) {
    initialize(); config.clear(); network.acquire(); ASSERT_EQ(tunnel.start(), 0);
    ASSERT_EQ(tunnel.poll(), 0);
    EXPECT_TRUE(backend.received.valid()); EXPECT_EQ(backend.received.private_key, configuration().private_key);
    EXPECT_EQ(backend.received.endpoint.port, 51820);
    EXPECT_EQ(backend.received.addresses[0].address[15], 2);
    ASSERT_EQ(tunnel.deinit(), 0); EXPECT_FALSE(backend.received.valid());
    EXPECT_EQ(config.private_key, (std::array<uint8_t, 32>{}));
}
TEST_F(TunnelTest, LossAndReacquisitionReleaseBeforeRestart) {
    connect(); network.online = false;
    ASSERT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.live, 0); EXPECT_EQ(backend.stops, 1);
    for (int i = 0; i < 10; ++i) EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 1); network.acquire(); EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.live, 1); EXPECT_EQ(backend.starts, 2);
}
TEST_F(TunnelTest, AddressChangeAndCoalescedOutageInvalidateOldTunnel) {
    connect(); network.acquire(); // Also represents loss/reacquire within one network poll.
    ASSERT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.stops, 1); EXPECT_EQ(backend.starts, 1);
    ASSERT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 2); EXPECT_EQ(backend.live, 1);
}
TEST_F(TunnelTest, StationStopOverridesStaleAvailability) {
    connect(); network.active = false; ASSERT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.live, 0); EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 1);
}
TEST_F(TunnelTest, ExplicitStopIsIdempotentAndCannotResurrect) {
    connect(); EXPECT_EQ(tunnel.start(), 0); EXPECT_EQ(backend.starts, 1);
    ASSERT_EQ(tunnel.stop(), 0); ASSERT_EQ(tunnel.stop(), 0); EXPECT_EQ(backend.stops, 1);
    network.online = false; EXPECT_EQ(tunnel.poll(), 0); network.acquire();
    for (int i = 0; i < 100; ++i) EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 1); EXPECT_EQ(tunnel.status(), TunnelState::stopped);
    ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 2);
}
TEST_F(TunnelTest, StopBeforeNetworkAcquisitionCancelsDeferredStart) {
    initialize(); ASSERT_EQ(tunnel.start(), 0); ASSERT_EQ(tunnel.stop(), 0);
    network.acquire(); EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 0);
}
TEST_F(TunnelTest, PartialStartFailureRequiresCleanupAndExplicitRetry) {
    initialize(); network.acquire(); backend.start_error = -7;
    ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), -7);
    EXPECT_EQ(tunnel.last_error(), -7); EXPECT_EQ(backend.live, 1);
    EXPECT_EQ(tunnel.start(), -2); ASSERT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.live, 0);
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 1);
    backend.start_error = 0; ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 2); EXPECT_EQ(backend.live, 1);
}
TEST_F(TunnelTest, FailedExplicitStopRetainsCleanupWithoutReconnecting) {
    connect(); backend.stop_error = -8; EXPECT_EQ(tunnel.stop(), -8);
    EXPECT_EQ(tunnel.start(), -2); EXPECT_EQ(tunnel.status(), TunnelState::cleanup_pending);
    network.acquire();
    for (int i = 0; i < 10; ++i) EXPECT_EQ(tunnel.poll(), -8);
    EXPECT_EQ(backend.starts, 1); backend.stop_error = 0; ASSERT_EQ(tunnel.stop(), 0);
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 1);
    ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 2);
}
TEST_F(TunnelTest, OutageCleanupFailureBlocksRestartUntilReconciled) {
    connect(); network.online = false; backend.stop_error = -8;
    EXPECT_EQ(tunnel.poll(), -8); network.acquire(); EXPECT_EQ(tunnel.poll(), -8);
    EXPECT_EQ(backend.starts, 1); backend.stop_error = 0;
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 1);
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.starts, 2); EXPECT_EQ(backend.live, 1);
}
TEST_F(TunnelTest, BackendFailureNeverAutomaticallyRetries) {
    connect(); backend.state = WireGuardBackend::State::failed;
    EXPECT_EQ(tunnel.poll(), -3); EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(backend.live, 0);
    for (int i = 0; i < 20; ++i) EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 1); ASSERT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 2);
}
TEST_F(TunnelTest, DeinitFailureBlocksInitAndStartUntilCleanup) {
    connect(); backend.stop_error = -8; EXPECT_EQ(tunnel.deinit(), -8);
    EXPECT_EQ(tunnel.init(config), -2); EXPECT_EQ(tunnel.start(), -2);
    backend.stop_error = 0; EXPECT_EQ(tunnel.deinit(), 0);
    EXPECT_EQ(tunnel.deinit(), 0); EXPECT_EQ(tunnel.status(), TunnelState::uninitialized);
    EXPECT_EQ(tunnel.start(), -1); EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(tunnel.init(config), 0); EXPECT_EQ(tunnel.start(), 0); EXPECT_EQ(tunnel.poll(), 0);
    EXPECT_EQ(backend.starts, 2);
}
TEST_F(TunnelTest, ThousandStartupShutdownCyclesHaveSingleResourceOwnership) {
    network.acquire();
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(tunnel.init(config), 0); ASSERT_EQ(tunnel.start(), 0);
        ASSERT_EQ(tunnel.poll(), 0); ASSERT_EQ(backend.live, 1);
        ASSERT_EQ(tunnel.deinit(), 0); ASSERT_EQ(backend.live, 0);
    }
    EXPECT_EQ(backend.starts, 1000); EXPECT_EQ(backend.stops, 1000);
}
TEST(WireGuardTransport, DestructorReleasesOwnedBackendWithoutNetworkOwnership) {
    Network network; Backend backend; network.acquire(); auto config = configuration();
    { WireGuardTransport tunnel{network, backend}; ASSERT_EQ(tunnel.init(config), 0);
      ASSERT_EQ(tunnel.start(), 0); ASSERT_EQ(tunnel.poll(), 0); }
    EXPECT_EQ(backend.live, 0); EXPECT_EQ(backend.stops, 1); EXPECT_TRUE(network.active);
}
TEST(WireGuardTransport, MissingBackendFailsClosedWithoutClaimingConnectivity) {
    Network network; UnavailableWireGuardBackend backend; network.acquire();
    WireGuardTransport tunnel{network, backend}; auto config = configuration();
    ASSERT_EQ(tunnel.init(config), 0); ASSERT_EQ(tunnel.start(), 0);
    EXPECT_EQ(tunnel.poll(), -6); EXPECT_NE(tunnel.status(), TunnelState::up);
    EXPECT_EQ(tunnel.poll(), 0); EXPECT_EQ(tunnel.status(), TunnelState::stopped);
    EXPECT_EQ(tunnel.last_error(), -6);
}
} // namespace
