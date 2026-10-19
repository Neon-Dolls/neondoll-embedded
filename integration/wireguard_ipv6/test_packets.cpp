#include <gtest/gtest.h>
#include "harness.h"
#include <array>
#include <vector>
#include <cstring>
#include <limits>
#include <arpa/inet.h>
namespace {
std::vector<uint8_t> packet(size_t payload=8,const char *source="fd00::1",const char *destination="fd00::2") {
    std::vector<uint8_t> p(40+payload,0);p[0]=0x60;p[4]=payload>>8;p[5]=payload;p[6]=59;p[7]=64;
    EXPECT_EQ(inet_pton(AF_INET6,source,p.data()+8),1);EXPECT_EQ(inet_pton(AF_INET6,destination,p.data()+24),1);
    for(size_t i=40;i<p.size();++i)p[i]=uint8_t(i);
    return p;
}
class Packets : public testing::Test {
    void SetUp() override { ASSERT_EQ(nd_setup("fd00::2","fd00::","ffff:ffff:ffff:ffff::"),0); }
    void TearDown() override { nd_cleanup();EXPECT_TRUE(nd_default_unchanged()); }
};
TEST_F(Packets, AssignsIpv6HostAddressAndRegistersOutputWithoutIpv4OrDefaultRoute) {
    EXPECT_TRUE(nd_address_correct("fd00::2"));EXPECT_TRUE(nd_output_registered());EXPECT_TRUE(nd_default_unchanged());
}
TEST_F(Packets, Ipv6TransmitUsesIpv4OuterUdp) {
    auto p=packet();EXPECT_EQ(nd_transmit(p.data(),p.size(),0),0);EXPECT_TRUE(nd_outer_ipv4());
}
TEST_F(Packets, TransmitReadsChainedPbufHeader) {
    auto p=packet();EXPECT_EQ(nd_transmit(p.data(),p.size(),1),0);EXPECT_TRUE(nd_outer_ipv4());
}
TEST_F(Packets, TransmitRejectsOversizeBeforeEncryptedBufferAllocation) {
    auto p=packet(1380);EXPECT_EQ(nd_transmit(p.data(),p.size(),0),0); // 1420-byte MTU.
    p=packet(1381);EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
}
TEST_F(Packets, TransmitRejectsUnallowedDestination) {
    auto p=packet(8,"fd00::1","fd01::1");EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
}
TEST_F(Packets, TransmitRejectsBadVersionLengthAndTruncatedHeader) {
    auto p=packet();p[0]=0x40;EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
    p=packet();p[5]=9;EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
    p=packet();p[5]=7;EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
    for(size_t len=0;len<40;++len) {p=packet();EXPECT_NE(nd_transmit(p.data(),len,0),0);}
}
TEST_F(Packets, AuthenticatedIpv6ReceiveTrimsWireguardPadding) {
    auto p=packet(9);ASSERT_TRUE(nd_receive(p.data(),p.size(),0,15));
    std::array<uint8_t,2048> received{};EXPECT_EQ(nd_received(received.data()),p.size());
    EXPECT_EQ(std::memcmp(received.data(),p.data(),p.size()),0);
}
TEST_F(Packets, RejectsWrongSourceEvenWhenDestinationIsAllowed) {
    auto p=packet(8,"fd01::1","fd00::2");EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0));
    p=packet(8,"fd00::1","fd01::2");EXPECT_TRUE(nd_receive(p.data(),p.size(),0,0)); // Filter is SOURCE.
}
TEST_F(Packets, InvalidAuthenticationCannotDeliver) {
    auto p=packet();EXPECT_FALSE(nd_receive(p.data(),p.size(),1,0));
}
TEST_F(Packets, ReceiveRejectsAllTruncatedFixedHeaders) {
    auto p=packet();for(size_t len=0;len<40;++len)EXPECT_FALSE(nd_receive(p.data(),len,0,0))<<len;
}
TEST_F(Packets, ReceiveRejectsBadVersionOversizePayloadAndTruncatedPayload) {
    auto p=packet();p[0]=0x70;EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0));
    p=packet();p[4]=0xff;p[5]=0xff;EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0));
    p=packet();p[5]=9;EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0));
    p=packet();EXPECT_FALSE(nd_receive(p.data(),p.size(),0,16));
}
TEST_F(Packets, EmptyPayloadIsValidButJumbogramsAreUnsupported) {
    auto p=packet(0);EXPECT_TRUE(nd_receive(p.data(),p.size(),0,0));
    EXPECT_EQ(nd_transmit(p.data(),p.size(),0),0);
    p[6]=0;EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0));EXPECT_NE(nd_transmit(p.data(),p.size(),0),0);
}
TEST(Ipv6Length, PayloadLimitExcludesFortyByteBaseHeader) {
    for(size_t payload:{size_t{65535-40},size_t{65535-39},size_t{65535}}) {
        auto p=packet(payload);
        EXPECT_EQ(nd_validate_ip6_length(p.data(),p.size(),0),40+payload);
        EXPECT_EQ(nd_validate_ip6_length(p.data(),p.size(),1),40+payload);
    }
}
TEST(Ipv6Length, MaximumPayloadStillRequiresCompletePacket) {
    auto p=packet(65535);
    for(size_t bytes:{size_t{39},size_t{40},size_t{65535},p.size()-1}) {
        EXPECT_EQ(nd_validate_ip6_length(p.data(),bytes,0),0u);
        EXPECT_EQ(nd_validate_ip6_length(p.data(),bytes,1),0u);
    }
}
TEST(Ipv6Length, MaximumPayloadDoesNotRelaxPaddingOrBufferBounds) {
    auto p=packet(65535);const size_t length=p.size();p.resize(length+16);
    EXPECT_EQ(nd_validate_ip6_length(p.data(),length+15,1),length);
    EXPECT_EQ(nd_validate_ip6_length(p.data(),length+15,0),0u);
    EXPECT_EQ(nd_validate_ip6_length(p.data(),length+16,1),0u);
    EXPECT_EQ(nd_validate_ip6_length(p.data(),std::numeric_limits<size_t>::max(),1),0u);
}
TEST_F(Packets, NonByteAlignedPrefixMatchesEveryBoundaryBit) {
    for(int bits=1;bits<=128;++bits) {
        nd_cleanup();std::array<uint8_t,16> mask{};
        for(int i=0;i<bits;++i)mask[i/8]|=0x80>>(i%8);
        char masktext[64];ASSERT_NE(inet_ntop(AF_INET6,mask.data(),masktext,sizeof(masktext)),nullptr);
        ASSERT_EQ(nd_setup("fd00::2","fd00::",masktext),0);
        auto p=packet(8,"fd00::","fd00::");EXPECT_TRUE(nd_receive(p.data(),p.size(),0,0))<<bits;
        p[8+(bits-1)/8]^=0x80>>((bits-1)%8);EXPECT_FALSE(nd_receive(p.data(),p.size(),0,0))<<bits;
        if(bits<128){p=packet(8,"fd00::","fd00::");p[8+bits/8]^=0x80>>(bits%8);EXPECT_TRUE(nd_receive(p.data(),p.size(),0,0))<<bits;}
        p=packet(8,"fd00::","fd00::");p[24+(bits-1)/8]^=0x80>>((bits-1)%8);
        EXPECT_NE(nd_transmit(p.data(),p.size(),0),0)<<bits;
    }
}
TEST_F(Packets, RejectsDefaultNoncontiguousAndMixedFamilyMasks) {
    EXPECT_FALSE(nd_add_allowed("fd00::","::"));
    EXPECT_FALSE(nd_add_allowed("fd00::","ffff:0:ffff::"));
    EXPECT_FALSE(nd_add_allowed("fd00::","255.255.255.0"));
    EXPECT_TRUE(nd_add_allowed("fd01::","ffff:ffff:ffff:ffff:8000::"));
    auto p=packet(8,"fd01::1","fd00::2");EXPECT_TRUE(nd_receive(p.data(),p.size(),0,0));
}
TEST_F(Packets, TransmitCiphertextDecryptsThroughFullUdpReceivePath) {
    auto p=packet(16);ASSERT_EQ(nd_transmit(p.data(),p.size(),1),0);
    ASSERT_TRUE(nd_receive_wire());std::array<uint8_t,2048> received{};
    EXPECT_EQ(nd_received(received.data()),p.size());
    EXPECT_EQ(std::memcmp(received.data(),p.data(),p.size()),0);
    EXPECT_FALSE(nd_receive_wire()); // Actual upstream replay protection.
}
TEST_F(Packets, InvalidPeerMasksFailActualConfigurationPath) {
    for(const char *mask:{"::","ffff:0:ffff::","255.255.255.0"}) {
        nd_cleanup();EXPECT_NE(nd_setup("fd00::2","fd00::",mask),0);
    }
    nd_cleanup();EXPECT_NE(nd_setup("fe80::2","fd00::","ffff:ffff:ffff:ffff::"),0);
}

TEST_F(Packets, RepeatedCreationAndCleanupDoNotChangeUnderlayDefault) {
    for(int i=0;i<100;++i){nd_cleanup();ASSERT_EQ(nd_setup("fd00::2","fd00::","ffff:ffff:ffff:ffff::"),0);EXPECT_TRUE(nd_default_unchanged());}
}
}
