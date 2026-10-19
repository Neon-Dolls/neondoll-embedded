#include <gtest/gtest.h>
#include <neondoll/esp32_wireguard.hpp>
#include "harness.h"
using namespace neondoll;
namespace {
WireGuardConfig config() {
    WireGuardConfig c;c.private_key.fill(0x11);c.peer_public_key.fill(0x22);
    c.endpoint.address={192,0,2,1};c.endpoint.port=51820;c.address_count=2;c.allowed_ip_count=2;
    c.addresses[0].address[0]=0xfd;c.addresses[0].address[15]=2;
    c.addresses[1]=c.addresses[0];c.addresses[1].address[15]=3;
    c.allowed_ips[0].address[0]=0xfd;c.allowed_ips[0].length=64;
    c.allowed_ips[1]=c.allowed_ips[0];c.allowed_ips[1].address[7]=1;c.allowed_ips[1].length=65;
    return c;
}
class BackendTest : public testing::Test {
protected:
    Esp32WireGuardBackend backend;
    void SetUp() override { nd_backend_reset(); }
    void TearDown() override { backend.stop();EXPECT_EQ(nd_backend_resources(),0);EXPECT_TRUE(nd_default_unchanged()); }
};
TEST_F(BackendTest, StartsOfflineUntilAuthenticatedPacketNotWifiOrKeyInitialization) {
    EXPECT_EQ(backend.status(),WireGuardBackend::State::down);auto c=config();ASSERT_EQ(backend.start(c),0);
    c.clear();EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
    nd_backend_peer_keys();EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
    EXPECT_FALSE(nd_backend_authenticate(1));EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
    EXPECT_TRUE(nd_backend_authenticate(0));EXPECT_EQ(backend.status(),WireGuardBackend::State::up);
    nd_backend_clock(181000);EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
    EXPECT_GT(nd_backend_zeroizations(),0);EXPECT_GT(nd_backend_decoded_wipes(),0);
}
TEST_F(BackendTest, PreservesBothAddressesPartialByteMaskAndIpv4Endpoint) {
    auto c=config();ASSERT_EQ(backend.start(c),0);EXPECT_TRUE(nd_backend_configured());
}
TEST_F(BackendTest, AuthenticatedReadinessHandlesClockWrapAndExpires) {
    nd_backend_clock(0xffffffffu-1000u);auto c=config();ASSERT_EQ(backend.start(c),0);
    nd_backend_peer_keys();ASSERT_TRUE(nd_backend_authenticate(0));
    nd_backend_clock(500);EXPECT_EQ(backend.status(),WireGuardBackend::State::up);
    nd_backend_clock(180000);EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
}
TEST_F(BackendTest, PartialFailuresAlwaysReleaseResourcesAndPermitRecovery) {
    for(const char *fault:{"platform","base64","netif","udp","bind","device_alloc","device_init","peer","allowed","connect"}) {
        nd_backend_fault(fault);auto c=config();EXPECT_NE(backend.start(c),0)<<fault;
        EXPECT_EQ(nd_backend_resources(),0)<<fault;EXPECT_EQ(backend.status(),WireGuardBackend::State::down);
        EXPECT_GT(nd_backend_zeroizations(),0);EXPECT_EQ(backend.stop(),0);
        nd_backend_fault("");ASSERT_EQ(backend.start(c),0)<<fault;EXPECT_EQ(backend.stop(),0);EXPECT_EQ(nd_backend_resources(),0);
    }
}
TEST_F(BackendTest, InvalidConfigAndMissingStationDoNotCreateResources) {
    auto c=config();c.private_key.fill(0);EXPECT_EQ(backend.start(c),-1);
    c=config();nd_backend_underlay(0);EXPECT_EQ(backend.start(c),-1);EXPECT_EQ(nd_backend_resources(),0);
    nd_backend_underlay(1);EXPECT_EQ(backend.start(c),0);
}
TEST_F(BackendTest, RepeatedStartAndSecondOwnerCannotDuplicateOrStopActiveResources) {
    auto c=config();ASSERT_EQ(backend.start(c),0);const int resources=nd_backend_resources();
    EXPECT_EQ(backend.start(c),-2);Esp32WireGuardBackend other;EXPECT_EQ(other.start(c),-2);
    EXPECT_EQ(other.stop(),0);EXPECT_EQ(nd_backend_resources(),resources);
    ASSERT_EQ(backend.stop(),0);EXPECT_EQ(other.start(c),0);EXPECT_EQ(other.stop(),0);
}
TEST_F(BackendTest, StopCancelsProtocolTimerAndCannotResurrect) {
    auto c=config();ASSERT_EQ(backend.start(c),0);EXPECT_EQ(backend.stop(),0);
    EXPECT_EQ(backend.stop(),0);EXPECT_FALSE(nd_backend_tick());EXPECT_EQ(nd_backend_resources(),0);
    EXPECT_EQ(backend.status(),WireGuardBackend::State::down);EXPECT_EQ(backend.start(c),0);
}
TEST_F(BackendTest, RemovedUnderlayDoesNotDereferenceStaleNetifAndCleanupSucceeds) {
    auto c=config();ASSERT_EQ(backend.start(c),0);nd_backend_underlay(0);
    EXPECT_EQ(backend.status(),WireGuardBackend::State::starting);
    EXPECT_TRUE(nd_backend_tick()); // Actual WG timer may try sending; removed underlay is rejected.
    EXPECT_EQ(backend.stop(),0);EXPECT_EQ(nd_backend_resources(),0);
    nd_backend_underlay(1);EXPECT_EQ(backend.start(c),0);
}
TEST_F(BackendTest, ThousandStartStopCyclesWipeOwnedDevicesAndRetainNoResources) {
    auto c=config();for(int i=0;i<1000;++i){ASSERT_EQ(backend.start(c),0);ASSERT_EQ(backend.stop(),0);ASSERT_EQ(nd_backend_resources(),0);}
    EXPECT_GE(nd_backend_zeroizations(),2000);
}
TEST_F(BackendTest, DestructorCancelsAllBackendCallbacksAndErasesDevice) {
    auto c=config();{Esp32WireGuardBackend local;ASSERT_EQ(local.start(c),0);}
    EXPECT_EQ(nd_backend_resources(),0);EXPECT_FALSE(nd_backend_tick());
}
TEST_F(BackendTest, TransportNetworkLossAndRecoveryKeepIntentAndStopWins) {
    struct Network : NetworkConnection {
        bool up=true;uint64_t epoch=1;
        int poll() override{return 0;}int connect() override{return 0;}int disconnect() override{up=false;return 0;}
        int stop() override{up=false;return 0;}bool available()const override{return up;}
        bool running()const override{return up;}bool connection_requested()const override{return false;}
        uint64_t availability_epoch()const override{return epoch;}
    } network;
    WireGuardTransport tunnel{network,backend};auto c=config();ASSERT_EQ(tunnel.init(c),0);
    ASSERT_EQ(tunnel.start(),0);ASSERT_EQ(tunnel.poll(),0);EXPECT_GT(nd_backend_resources(),0);
    network.up=false;ASSERT_EQ(tunnel.poll(),0);EXPECT_EQ(nd_backend_resources(),0);
    network.up=true;++network.epoch;ASSERT_EQ(tunnel.poll(),0);EXPECT_GT(nd_backend_resources(),0);
    ASSERT_EQ(tunnel.stop(),0);++network.epoch;ASSERT_EQ(tunnel.poll(),0);EXPECT_EQ(nd_backend_resources(),0);
}
}
