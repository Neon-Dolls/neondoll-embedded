#include <gtest/gtest.h>
#include <neondoll/esp32_network.hpp>
#include "network_fake.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>

namespace {
using Network = neondoll::Esp32NetworkAvailability;
using network_fake::state;
using network_fake::emit;
using network_fake::got_ip;
class NetworkTest : public testing::Test {
protected:
    Network network;
    std::vector<bool> changes;
    std::vector<Network::Change> kinds;
    void SetUp() override { network_fake::reset(); }
    static void changed(void* arg,bool available) {
        auto& self=*static_cast<NetworkTest*>(arg);
        self.changes.push_back(available);self.kinds.push_back(self.network.last_change());
    }
    void ready(uint8_t retries=3) {
        ASSERT_EQ(network.init(),0);
        ASSERT_EQ(network.set_callback(changed,this),0);
        ASSERT_EQ(network.start("test-ssid","test-password",retries),0);
        ASSERT_EQ(network.connect(),0);
        ASSERT_EQ(network.poll(),0); // Initial offline snapshot, never event-task delivery.
        ASSERT_EQ(changes,std::vector<bool>{false});
    }
    void TearDown() override {
        state().fail.clear(); network.deinit();
        Network recover;
        ASSERT_EQ(recover.init(),0);ASSERT_EQ(recover.deinit(),0);
        EXPECT_EQ(state().queues,0);EXPECT_EQ(state().interfaces,0);
        EXPECT_TRUE(state().handlers.empty());EXPECT_FALSE(state().wifi);
    }
};
TEST_F(NetworkTest, InitAndCallbackRegistrationDoNotStartRadio) {
    EXPECT_EQ(network.connect(),-1);EXPECT_EQ(network.poll(),-1);
    EXPECT_EQ(network.set_callback(changed,this),-1);
    ASSERT_EQ(network.init(),0);ASSERT_EQ(network.init(),0);
    EXPECT_TRUE(state().calls.empty());EXPECT_EQ(state().queues,1);
    ASSERT_EQ(network.set_callback(changed,this),0);EXPECT_TRUE(changes.empty());
    ASSERT_EQ(network.poll(),0);EXPECT_EQ(changes,std::vector<bool>{false});
    EXPECT_EQ(network.deinit(),0);EXPECT_EQ(network.deinit(),0);
    EXPECT_EQ(network.poll(),-1);EXPECT_EQ(changes.size(),1u);
}
TEST_F(NetworkTest, AssociationIsNotAvailabilityAndIpChangesAreDistinct) {
    ready();
    emit(WIFI_EVENT,WIFI_EVENT_STA_START);emit(WIFI_EVENT,WIFI_EVENT_STA_CONNECTED);
    ASSERT_EQ(network.poll(),0);EXPECT_FALSE(network.available());
    got_ip(1);EXPECT_EQ(changes.size(),1u); // IDF callback queues only.
    ASSERT_EQ(network.poll(),0);EXPECT_EQ(changes,(std::vector<bool>{false,true}));
    EXPECT_EQ(network.ipv4_address(),1u);EXPECT_EQ(kinds.back(),Network::Change::available);
    got_ip(1);ASSERT_EQ(network.poll(),0);EXPECT_EQ(changes.size(),2u);
    got_ip(2);ASSERT_EQ(network.poll(),0);EXPECT_EQ(changes,(std::vector<bool>{false,true,true}));
    EXPECT_EQ(kinds.back(),Network::Change::changed);
}
TEST_F(NetworkTest, UnexpectedDisconnectReconnectsWithBoundedPolicy) {
    ready(2);got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);ASSERT_EQ(network.poll(),0);
    EXPECT_FALSE(network.available());EXPECT_EQ(state().connect_calls,2);
    got_ip(2);ASSERT_EQ(network.poll(),0);EXPECT_TRUE(network.available());
    EXPECT_EQ(changes,(std::vector<bool>{false,true,false,true}));
    for(int i=0;i<3;++i){emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);network.poll();}
    EXPECT_EQ(state().connect_calls,4);EXPECT_NE(network.last_error(),0);
    for(int i=0;i<10;++i){emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);network.poll();}
    EXPECT_EQ(state().connect_calls,4);
    ASSERT_EQ(network.connect(),0);got_ip(3);EXPECT_EQ(network.poll(),0);
}
TEST_F(NetworkTest, ExplicitDisconnectSuppressesQueuedIpAndReconnect) {
    ready();got_ip(1);
    ASSERT_EQ(network.disconnect(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);ASSERT_EQ(network.poll(),0);
    EXPECT_FALSE(network.available());EXPECT_EQ(state().connect_calls,1);
    ASSERT_EQ(network.connect(),0);got_ip(2);ASSERT_EQ(network.poll(),0);
    EXPECT_TRUE(network.available());
}
TEST_F(NetworkTest, LostIpAndDelayedLostIpTimer) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(IP_EVENT,IP_EVENT_STA_LOST_IP);ASSERT_EQ(network.poll(),0);
    EXPECT_TRUE(network.available()); // Delayed timer while the interface has a new address.
    state().address=0;emit(IP_EVENT,IP_EVENT_STA_LOST_IP);ASSERT_EQ(network.poll(),0);
    EXPECT_FALSE(network.available());
    got_ip(2);ASSERT_EQ(network.poll(),0);EXPECT_TRUE(network.available());
}
TEST_F(NetworkTest, ForeignAndMalformedIpEventsAreIgnored) {
    ready();
    esp_netif_t foreign{2};ip_event_got_ip_t event{&foreign,{{1},{0},{0}},true};
    emit(IP_EVENT,IP_EVENT_STA_GOT_IP,&event);emit(IP_EVENT,IP_EVENT_STA_GOT_IP);
    got_ip(0);ASSERT_EQ(network.poll(),0);EXPECT_FALSE(network.available());
}
TEST_F(NetworkTest, ConnectionAndReconnectionErrorsNeedExplicitRetry) {
    ready();ASSERT_EQ(network.disconnect(),0);
    state().fail="connect";EXPECT_EQ(network.connect(),-3);
    EXPECT_EQ(network.last_error(),ESP_FAIL);
    const int calls=state().connect_calls;
    emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);EXPECT_EQ(network.poll(),-3);
    EXPECT_EQ(state().connect_calls,calls);
    state().fail.clear();ASSERT_EQ(network.connect(),0);got_ip(1);ASSERT_EQ(network.poll(),0);
    state().fail="connect";emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);
    EXPECT_EQ(network.poll(),-3);EXPECT_FALSE(network.available());
}
TEST_F(NetworkTest, InitAndStartFailuresReleaseOnlyOwnedResources) {
    state().queue_fail=true;EXPECT_EQ(network.init(),-3);
    state().queue_fail=false;ASSERT_EQ(network.init(),0);
    for(const auto* fail:{"netif_init","loop_create","wifi_init","storage_ram","mode_sta",
                         "wifi_config","register_wifi","register_ip","wifi_start"}) {
        state().fail=fail;EXPECT_EQ(network.start("ssid","password"),-3)<<fail;
        EXPECT_FALSE(state().wifi)<<fail;EXPECT_EQ(state().interfaces,0)<<fail;
        EXPECT_TRUE(state().handlers.empty())<<fail;EXPECT_FALSE(state().loop)<<fail;
    }
    state().fail.clear();state().netif_fail=true;EXPECT_EQ(network.start("ssid","password"),-3);
    state().netif_fail=false;ASSERT_EQ(network.start("ssid","password"),0);
    EXPECT_EQ(state().ssid,"ssid");EXPECT_EQ(state().password,"password");
}
TEST_F(NetworkTest, CredentialsAreValidatedBeforeAcquiringResources) {
    ASSERT_EQ(network.init(),0);
    EXPECT_EQ(network.start(nullptr,"p"),-1);EXPECT_EQ(network.start("ssid",nullptr),-1);
    EXPECT_EQ(network.start("","p"),-1);
    EXPECT_EQ(network.start(std::string(33,'s').c_str(),"p"),-1);
    EXPECT_EQ(network.start("s",std::string(65,'p').c_str()),-1);
    EXPECT_TRUE(state().calls.empty());
    ASSERT_EQ(network.start(std::string(32,'s').c_str(),""),0);
    EXPECT_EQ(state().ssid.size(),32u);EXPECT_EQ(network.start("s","p"),-2);
}
TEST_F(NetworkTest, ApplicationOwnedLoopAndInterfaceArePreserved) {
    state().loop=true;state().external_netif=true;
    ASSERT_EQ(network.init(),0);EXPECT_EQ(network.start("ssid","p"),-3);
    EXPECT_TRUE(state().loop);EXPECT_EQ(state().interfaces,0);
    state().external_netif=false;ASSERT_EQ(network.start("ssid","p"),0);
    ASSERT_EQ(network.deinit(),0);EXPECT_TRUE(state().loop);
    state().loop=false;
}
TEST_F(NetworkTest, OneStationOwnerAtATime) {
    ASSERT_EQ(network.init(),0);Network other;EXPECT_EQ(other.init(),-2);
    EXPECT_EQ(other.deinit(),0);EXPECT_EQ(state().queues,1);
    ASSERT_EQ(network.deinit(),0);ASSERT_EQ(other.init(),0);EXPECT_EQ(other.deinit(),0);
}
TEST_F(NetworkTest, RepeatedCyclesHaveNoDuplicateHandlersOrQueues) {
    for(int i=0;i<100;++i) {
        ASSERT_EQ(network.init(),0);ASSERT_EQ(network.start("ssid","p"),0);
        ASSERT_EQ(network.connect(),0);got_ip(1);ASSERT_EQ(network.poll(),0);
        EXPECT_EQ(state().handlers.size(),2u);EXPECT_EQ(state().queues,1);
        ASSERT_EQ(network.deinit(),0);EXPECT_TRUE(state().handlers.empty());
        EXPECT_EQ(state().interfaces,0);EXPECT_EQ(state().queues,0);
    }
}
TEST_F(NetworkTest, QueuedAndLateHandlersCannotResurrectStoppedOrReinitializedOwner) {
    ready();const auto old=state().handlers;
    got_ip(1);ASSERT_EQ(network.deinit(),0);
    for(const auto& handler:old)handler.callback(handler.arg,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
    ASSERT_EQ(network.init(),0);ASSERT_EQ(network.set_callback(changed,this),0);
    ASSERT_EQ(network.start("ssid","p"),0);ASSERT_EQ(network.connect(),0);
    ip_event_got_ip_t event{&state().netif,{{7},{0},{0}},true};
    for(const auto& handler:old)handler.callback(handler.arg,IP_EVENT,IP_EVENT_STA_GOT_IP,&event);
    ASSERT_EQ(network.poll(),0);EXPECT_FALSE(network.available());
    got_ip(2);ASSERT_EQ(network.poll(),0);EXPECT_TRUE(network.available());
}
TEST_F(NetworkTest, LateHandlersAfterDestructionAndUnregisterFailureAreSafe) {
    auto owner=std::make_unique<Network>();ASSERT_EQ(owner->init(),0);
    int notifications=0;
    ASSERT_EQ(owner->set_callback([](void* arg,bool){++*static_cast<int*>(arg);},&notifications),0);
    ASSERT_EQ(owner->poll(),0);
    ASSERT_EQ(owner->start("ssid","p"),0);const auto old=state().handlers;
    state().fail="unregister_wifi";owner.reset();
    for(const auto& handler:old)handler.callback(handler.arg,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
    EXPECT_EQ(notifications,1);
    EXPECT_EQ(network.init(),-3); // No accumulation of failed registrations.
    EXPECT_EQ(state().handlers.size(),1u);
    state().fail.clear();ASSERT_EQ(network.init(),0);ASSERT_EQ(network.start("ssid","p"),0);
    EXPECT_EQ(state().handlers.size(),2u);
}
TEST_F(NetworkTest, TeardownFailuresRemainSafeAndRecoverOnNextInit) {
    for(const auto* failure:{"unregister_wifi","unregister_ip","wifi_stop","wifi_deinit","loop_delete"}) {
        ASSERT_EQ(network.init(),0);ASSERT_EQ(network.start("ssid","p"),0);
        const auto old=state().handlers;state().fail=failure;
        EXPECT_EQ(network.deinit(),-3)<<failure;
        for(const auto& handler:old)handler.callback(handler.arg,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
        state().fail.clear();ASSERT_EQ(network.init(),0)<<failure;ASSERT_EQ(network.deinit(),0);
    }
}
TEST_F(NetworkTest, EventDeliveryRacingShutdownNeverCallsApplicationCode) {
    ready();const auto handler=state().handlers.front();std::atomic<bool> done{false}, seen{false};
    std::thread sender([&]{
        handler.callback(handler.arg,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
        seen=true;
        while(!done.load())handler.callback(handler.arg,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,nullptr);
    });
    while(!seen.load())std::this_thread::yield();
    EXPECT_EQ(network.deinit(),0);done=true;sender.join();
    EXPECT_EQ(changes,std::vector<bool>{false});
}
TEST_F(NetworkTest, OverflowFailsClosedWithoutUnboundedReconnects) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    for(int i=0;i<30;++i)emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);
    EXPECT_EQ(network.poll(),-4);EXPECT_FALSE(network.available());
    EXPECT_EQ(state().connect_calls,1);
    ASSERT_EQ(network.connect(),0);got_ip(2);ASSERT_EQ(network.poll(),0);
    EXPECT_TRUE(network.available());
}
TEST_F(NetworkTest, CallbackRemovalAndStopPreventFurtherNotifications) {
    ready();ASSERT_EQ(network.set_callback(nullptr,nullptr),0);
    got_ip(1);ASSERT_EQ(network.poll(),0);EXPECT_EQ(changes.size(),1u);
    ASSERT_EQ(network.stop(),0);ASSERT_EQ(network.stop(),0);
    ASSERT_EQ(network.poll(),0);EXPECT_FALSE(network.available());
    ASSERT_EQ(network.start("ssid","p"),0);ASSERT_EQ(network.connect(),0);
}
TEST_F(NetworkTest, CallbacksCannotRecursivelyDriveLifecycle) {
    ASSERT_EQ(network.init(),0);
    ASSERT_EQ(network.set_callback([](void* arg,bool){
        auto& net=*static_cast<Network*>(arg);
        EXPECT_EQ(net.poll(),-2);EXPECT_EQ(net.deinit(),-2);
        EXPECT_EQ(net.start("s","p"),-2);EXPECT_EQ(net.connect(),-2);
    },&network),0);
    EXPECT_EQ(network.poll(),0);
}
TEST_F(NetworkTest, UnexpectedStopReportsOfflineAndCanRestart) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_STOP);ASSERT_EQ(network.poll(),0);
    EXPECT_FALSE(network.available());EXPECT_EQ(network.connect(),-1);
    ASSERT_EQ(network.start("ssid","p"),0);ASSERT_EQ(network.connect(),0);
}

TEST_F(NetworkTest, RuntimeIdentityEntropyCompletesBeforeRadioStart) {
    struct Storage : neondoll::Storage {
        std::string data;
        int init() override{return 0;}int deinit() override{return 0;}
        std::ptrdiff_t read(const char*,void*,size_t) override{return -1;}
        int write(const char*,const void* in,size_t size) override{data.assign(static_cast<const char*>(in),size);return 0;}
    } storage;
    struct Entropy : neondoll::EntropySource {
        int init() override{return 0;}int deinit() override{return 0;}
        int get_random(void* out,size_t size) override {
            EXPECT_TRUE(state().calls.empty());EXPECT_FALSE(state().wifi);EXPECT_FALSE(state().started);
            std::fill_n(static_cast<unsigned char*>(out),size,0xab);return 0;
        }
    } entropy;
    struct Clock : neondoll::Clock {int init() override{return 0;}int deinit() override{return 0;}uint64_t now_ms() override{return 0;}} clock;
    struct Logger : neondoll::Logger {int init() override{return 0;}int deinit() override{return 0;}int log(int,const char*,const char*) override{return 0;}} logger;
    neondoll::PlatformContext ctx;ctx.storage=&storage;ctx.entropy=&entropy;ctx.network=&network;ctx.clock=&clock;ctx.logger=&logger;
    ASSERT_EQ(neondoll_init(&ctx),0);const auto identity=storage.data;
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);ASSERT_EQ(network.poll(),0);
    got_ip(2);ASSERT_EQ(network.poll(),0);EXPECT_EQ(storage.data,identity);
    EXPECT_EQ(neondoll_deinit(&ctx),0);
}
}

TEST_F(NetworkTest, FailedExplicitDisconnectStillSuppressesAutomaticReconnect) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    state().fail="disconnect";EXPECT_EQ(network.disconnect(),-3);
    got_ip(2);emit(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED);
    EXPECT_EQ(network.poll(),-3);EXPECT_FALSE(network.available());
    EXPECT_EQ(state().connect_calls,1);
    state().fail.clear();ASSERT_EQ(network.connect(),0);
    got_ip(3);EXPECT_EQ(network.poll(),0);EXPECT_TRUE(network.available());
}

TEST_F(NetworkTest, DelayedStopNotificationDoesNotSkipStoppingRunningDriver) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_STOP);
    // The queued notification is not authoritative about the driver's current
    // state (e.g. a delayed event after the driver has become active again).
    state().started=true;
    ASSERT_EQ(network.poll(),0);
    EXPECT_FALSE(network.available());EXPECT_EQ(network.connect(),-1);
    const auto stops=std::count(state().calls.begin(),state().calls.end(),"wifi_stop");
    ASSERT_EQ(network.start("ssid","p"),0);
    EXPECT_EQ(std::count(state().calls.begin(),state().calls.end(),"wifi_stop"),stops+1);
    EXPECT_EQ(state().handlers.size(),2u);EXPECT_EQ(state().interfaces,1);
    EXPECT_EQ(state().queues,1);ASSERT_EQ(network.connect(),0);
    got_ip(2);EXPECT_EQ(network.poll(),0);EXPECT_TRUE(network.available());
}

TEST_F(NetworkTest, StopThenFailedDeinitRetriesActualDriverStopBeforeRestart) {
    ready();got_ip(1);ASSERT_EQ(network.poll(),0);
    emit(WIFI_EVENT,WIFI_EVENT_STA_STOP);ASSERT_EQ(network.poll(),0);
    state().fail="wifi_deinit";
    EXPECT_EQ(network.start("ssid","p"),-3);
    EXPECT_TRUE(state().wifi);EXPECT_FALSE(state().started);
    EXPECT_EQ(state().interfaces,1);EXPECT_EQ(state().queues,1);
    EXPECT_TRUE(state().handlers.empty());EXPECT_FALSE(network.available());
    const auto creations=std::count(state().calls.begin(),state().calls.end(),"netif_create");
    const auto stops=std::count(state().calls.begin(),state().calls.end(),"wifi_stop");
    // Cleanup failed, so its cached stop result cannot establish the current
    // driver state for the next cleanup attempt.
    state().started=true;state().fail.clear();
    ASSERT_EQ(network.start("ssid","p"),0);
    EXPECT_EQ(std::count(state().calls.begin(),state().calls.end(),"wifi_stop"),stops+1);
    EXPECT_EQ(std::count(state().calls.begin(),state().calls.end(),"netif_create"),creations+1);
    EXPECT_EQ(state().handlers.size(),2u);EXPECT_EQ(state().interfaces,1);
    EXPECT_EQ(state().queues,1);ASSERT_EQ(network.connect(),0);
    got_ip(2);ASSERT_EQ(network.poll(),0);
    EXPECT_EQ(changes,(std::vector<bool>{false,true,false,true}));
}

TEST_F(NetworkTest, StopAndFailedShutdownRecoverForNextOwnerWithoutLeaks) {
    ready();emit(WIFI_EVENT,WIFI_EVENT_STA_STOP);ASSERT_EQ(network.poll(),0);
    state().fail="wifi_deinit";EXPECT_EQ(network.deinit(),-3);
    EXPECT_EQ(state().queues,0);EXPECT_EQ(state().interfaces,1);
    EXPECT_TRUE(state().handlers.empty());EXPECT_TRUE(state().wifi);
    state().started=true;state().fail.clear();
    Network next;
    ASSERT_EQ(next.init(),0);
    EXPECT_FALSE(state().wifi);EXPECT_EQ(state().interfaces,0);
    ASSERT_EQ(next.start("ssid","p"),0);ASSERT_EQ(next.connect(),0);
    EXPECT_EQ(state().queues,1);EXPECT_EQ(state().interfaces,1);
    EXPECT_EQ(state().handlers.size(),2u);
    ASSERT_EQ(next.deinit(),0);
    EXPECT_EQ(state().queues,0);EXPECT_EQ(state().interfaces,0);
    EXPECT_TRUE(state().handlers.empty());EXPECT_FALSE(state().wifi);
}
