#include <gtest/gtest.h>
#include <neondoll/network_supervisor.hpp>
#include <limits>

namespace {
using Supervisor = neondoll::NetworkSupervisor;
using State = Supervisor::State;
struct Clock : neondoll::Clock {
    uint64_t now=0;
    int init() override{return 0;}int deinit() override{return 0;}
    uint64_t now_ms() override{return now;}
};
struct Port : neondoll::NetworkConnection {
    bool up=false, running_=true, requested=false;
    int connects=0, disconnects=0, stops=0, polls=0;
    int connect_error=0, disconnect_error=0, stop_error=0, poll_error=0;
    uint64_t epoch=0;bool previously_up=false;
    int poll() override{++polls;if(up && !previously_up)++epoch;previously_up=up;return poll_error;}
    int connect() override{++connects;if(!connect_error)requested=true;return connect_error;}
    int disconnect() override{++disconnects;requested=false;up=false;return disconnect_error;}
    int stop() override{++stops;running_=false;requested=false;up=false;return stop_error;}
    bool available() const override{return up;}
    bool running() const override{return running_;}
    bool connection_requested() const override{return requested;}
    uint64_t availability_epoch() const override{return epoch;}
};
class RecoveryTest : public testing::Test {
protected:
    Clock clock;Port port;
    Supervisor supervisor{port,clock,{10,40,100}};
    void connecting(){ASSERT_EQ(supervisor.start(),0);ASSERT_EQ(supervisor.poll(),0);ASSERT_EQ(port.connects,1);}
    void online(){connecting();port.up=true;ASSERT_EQ(supervisor.poll(),0);ASSERT_EQ(supervisor.state(),State::online);}
};
TEST_F(RecoveryTest, InitialConnectionIsDeferredAndSuccessResetsPolicy) {
    ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(port.connects,0);
    ASSERT_EQ(supervisor.poll(),0);EXPECT_EQ(supervisor.state(),State::connecting);
    port.up=true;ASSERT_EQ(supervisor.poll(),0);
    EXPECT_EQ(supervisor.state(),State::online);EXPECT_EQ(supervisor.failures(),0u);
    EXPECT_EQ(supervisor.retry_delay_ms(),0u);EXPECT_EQ(supervisor.last_error(),0);
}
TEST_F(RecoveryTest, RetryExhaustionHasCappedExponentialBackoff) {
    connecting();
    for(uint64_t delay:{10u,20u,40u,40u,40u}) {
        port.requested=false;port.poll_error=-3;
        EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(supervisor.state(),State::backoff);
        EXPECT_EQ(supervisor.retry_delay_ms(),delay);
        const int attempts=port.connects;
        clock.now+=delay-1;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,attempts);
        ++clock.now;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,attempts+1);
    }
    EXPECT_EQ(supervisor.failures(),5u);
}
TEST_F(RecoveryTest, SyncConnectionFailuresAreDelayedWithoutBusyLoops) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(supervisor.retry_delay_ms(),10u);
    for(int i=0;i<100;++i)EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,1);clock.now=10;
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
    EXPECT_EQ(supervisor.retry_delay_ms(),20u);
}
TEST_F(RecoveryTest, SuccessfulIpResetsDelayAfterExtendedOutage) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);
    for(int i=0;i<1000;++i) {supervisor.poll();clock.now+=40;}
    EXPECT_EQ(supervisor.retry_delay_ms(),40u);EXPECT_EQ(supervisor.failures(),1000u);
    port.connect_error=0;ASSERT_EQ(supervisor.poll(),0);
    port.up=true;ASSERT_EQ(supervisor.poll(),0);
    EXPECT_EQ(supervisor.failures(),0u);EXPECT_EQ(supervisor.last_error(),0);
    port.up=false;port.requested=false;
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(supervisor.retry_delay_ms(),10u);
}
TEST_F(RecoveryTest, ConnectionTimeoutCancelsAssociationAndBacksOff) {
    connecting();clock.now=99;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.disconnects,0);
    clock.now=100;EXPECT_EQ(supervisor.poll(),-5);
    EXPECT_EQ(port.disconnects,1);EXPECT_FALSE(port.requested);
    EXPECT_EQ(supervisor.retry_delay_ms(),10u);
    clock.now=109;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=110;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, IpAcquisitionWinsAtTimeoutBoundary) {
    connecting();clock.now=100;port.up=true;
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(supervisor.state(),State::online);
    EXPECT_EQ(port.disconnects,0);
}
TEST_F(RecoveryTest, UnexpectedLossCancelsAnyAdapterRetryBurst) {
    online();port.up=false;port.requested=true;
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_FALSE(port.requested);
    EXPECT_EQ(port.disconnects,1);EXPECT_EQ(supervisor.retry_delay_ms(),10u);
    clock.now=10;ASSERT_EQ(supervisor.poll(),0);port.up=true;
    ASSERT_EQ(supervisor.poll(),0);EXPECT_EQ(supervisor.state(),State::online);
}
TEST_F(RecoveryTest, IntentionalDisconnectStaysPausedUntilExplicitRecovery) {
    online();ASSERT_EQ(supervisor.disconnect(),0);
    EXPECT_EQ(supervisor.state(),State::paused);
    clock.now=1000000;
    for(int i=0;i<100;++i)EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,1);
    ASSERT_EQ(supervisor.request_recovery(),0);ASSERT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,2);EXPECT_EQ(supervisor.state(),State::connecting);
}
TEST_F(RecoveryTest, ShutdownDuringBackoffNeverResurrectsConnection) {
    connecting();port.requested=false;EXPECT_EQ(supervisor.poll(),-3);
    ASSERT_EQ(supervisor.request_recovery(),0);
    ASSERT_EQ(supervisor.stop(),0);EXPECT_EQ(supervisor.request_recovery(),-1);
    clock.now=std::numeric_limits<uint64_t>::max();
    for(int i=0;i<100;++i)EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,1);EXPECT_EQ(supervisor.state(),State::stopped);
    EXPECT_EQ(supervisor.start(),-1); // Application must explicitly start the station first.
    port.running_=true;ASSERT_EQ(supervisor.start(),0);ASSERT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, ExplicitRecoveryRequestsCoalesceAndDoNotResetFailureStreak) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    for(int i=0;i<100;++i)ASSERT_EQ(supervisor.request_recovery(),0);
    EXPECT_EQ(port.connects,1);
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
    EXPECT_EQ(supervisor.failures(),2u);EXPECT_EQ(supervisor.retry_delay_ms(),20u);
}
TEST_F(RecoveryTest, RecoveryRequestDoesNotInterruptAnActiveAttemptOrHealthyConnection) {
    connecting();ASSERT_EQ(supervisor.request_recovery(),0);EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,1);port.up=true;ASSERT_EQ(supervisor.poll(),0);
    ASSERT_EQ(supervisor.request_recovery(),0);EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,1);EXPECT_EQ(port.disconnects,0);
}
TEST_F(RecoveryTest, RepeatedRecoveryCyclesDoNotAccumulatePolicyState) {
    online();
    for(int i=0;i<1000;++i) {
        port.up=false;port.requested=false;EXPECT_EQ(supervisor.poll(),-3);
        EXPECT_EQ(supervisor.retry_delay_ms(),10u);clock.now+=10;
        ASSERT_EQ(supervisor.poll(),0);port.up=true;ASSERT_EQ(supervisor.poll(),0);
        EXPECT_EQ(supervisor.state(),State::online);EXPECT_EQ(supervisor.failures(),0u);
    }
    EXPECT_EQ(port.connects,1001);
}
TEST_F(RecoveryTest, LargeClockJumpRunsOnlyOneAttempt) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    clock.now=1000000000;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, BackwardClockJumpRebasesWithoutImmediateRetry) {
    clock.now=100;port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);
    EXPECT_EQ(supervisor.poll(),-3);clock.now=105;EXPECT_EQ(supervisor.poll(),0);
    clock.now=0;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=4;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=5;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, ClockWrapIsConservativeAndDeadlineAdditionNeverOverflows) {
    clock.now=std::numeric_limits<uint64_t>::max()-5;
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    clock.now=0;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=9;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=10;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, DriverStopRequiresExplicitStationAndSupervisorRestart) {
    online();port.up=false;port.running_=false;
    EXPECT_EQ(supervisor.poll(),-1);EXPECT_EQ(supervisor.state(),State::stopped);
    EXPECT_EQ(supervisor.request_recovery(),-1);
    clock.now=10000;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    port.running_=true;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, FailedCancellationAndShutdownCannotRestartConnection) {
    online();port.up=false;port.disconnect_error=-3;
    EXPECT_EQ(supervisor.poll(),-3);clock.now=10;
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,1);
    port.disconnect_error=0;clock.now=30;
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
    port.stop_error=-3;EXPECT_EQ(supervisor.stop(),-3);
    clock.now=100000;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
    port.stop_error=0;EXPECT_EQ(supervisor.stop(),0);
}
TEST_F(RecoveryTest, FailedIntentionalDisconnectDoesNotEnableRetries) {
    online();port.disconnect_error=-3;EXPECT_EQ(supervisor.disconnect(),-3);
    clock.now=1000000;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    port.disconnect_error=0;ASSERT_EQ(supervisor.request_recovery(),0);
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
}
TEST(RecoveryPolicy, InvalidBoundsAndStoppedStationAreRejectedWithoutNetworkCalls) {
    for(const neondoll::NetworkRecoveryPolicy policy: {neondoll::NetworkRecoveryPolicy{0,10,10},
        neondoll::NetworkRecoveryPolicy{10,9,10},neondoll::NetworkRecoveryPolicy{1,10,0}}) {
        Port port;Clock clock;Supervisor supervisor{port,clock,policy};
        EXPECT_EQ(supervisor.start(),-1);EXPECT_EQ(port.connects,0);EXPECT_EQ(port.stops,0);
    }
    Port stopped;Clock clock;stopped.running_=false;
    Supervisor supervisor{stopped,clock};EXPECT_EQ(supervisor.start(),-1);
    EXPECT_EQ(stopped.connects,0);
}
TEST(RecoveryPolicy, MaximumBackoffDoesNotOverflowOnDoubling) {
    const auto maximum=std::numeric_limits<uint64_t>::max();
    Port port;Clock clock;Supervisor supervisor{port,clock,{maximum/2+1,maximum,1}};
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    EXPECT_EQ(supervisor.retry_delay_ms(),maximum/2+1);
    clock.now=maximum;EXPECT_EQ(supervisor.poll(),-3);
    EXPECT_EQ(supervisor.retry_delay_ms(),maximum);EXPECT_EQ(port.connects,2);
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
}
TEST(RecoveryPolicy, DestructionStopsOnlyAnOwnedConnection) {
    Port port;Clock clock;
    {Supervisor never_started{port,clock};}EXPECT_EQ(port.stops,0);
    {Supervisor active{port,clock};ASSERT_EQ(active.start(),0);}
    EXPECT_EQ(port.stops,1);EXPECT_FALSE(port.running_);
}
}

TEST(RecoveryPolicy, ElapsedClockCounterSaturatesWithoutWrapping) {
    const auto maximum=std::numeric_limits<uint64_t>::max();
    Port port;Clock clock;Supervisor supervisor{port,clock,{maximum,maximum,1}};
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    clock.now=maximum-1;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=0;EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,1);
    clock.now=10;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
    EXPECT_EQ(supervisor.poll(),0);EXPECT_EQ(port.connects,2);
}
TEST_F(RecoveryTest, RepeatedStartDoesNotResetBackoffOrCircumventDelay) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    clock.now=5;EXPECT_EQ(supervisor.start(),-2);EXPECT_EQ(supervisor.poll(),0);
    EXPECT_EQ(supervisor.retry_delay_ms(),10u);EXPECT_EQ(port.connects,1);
    clock.now=10;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(port.connects,2);
}

TEST_F(RecoveryTest, AcquisitionFollowedByLossInOnePollResetsBackoff) {
    port.connect_error=-3;ASSERT_EQ(supervisor.start(),0);EXPECT_EQ(supervisor.poll(),-3);
    clock.now=10;EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(supervisor.retry_delay_ms(),20u);
    port.connect_error=0;ASSERT_EQ(supervisor.request_recovery(),0);ASSERT_EQ(supervisor.poll(),0);
    ++port.epoch;port.requested=false; // Transient IP acquisition then loss in the next port poll.
    EXPECT_EQ(supervisor.poll(),-3);EXPECT_EQ(supervisor.retry_delay_ms(),10u);
    EXPECT_EQ(supervisor.failures(),1u);
}
