#include <gtest/gtest.h>
#include "packet.hpp"
#include <array>
TEST(InteropPacket, SharedPythonVector) {
    uint8_t out[16]{};ASSERT_TRUE(interop::make(out,sizeof(out),'Q','D',0x01020304));
    const uint8_t expected[]={0x4e,0x44,0x57,0x47,0x33,0x32,0x43,0x21,0x51,0x44,1,2,3,4,0x12,0x13};
    EXPECT_EQ(std::memcmp(out,expected,sizeof(out)),0);
}
TEST(InteropPacket, BoundariesAndTampering) {
    std::array<uint8_t,1374> data{};uint32_t seq=0;
    for(size_t n:{14u,64u,1232u,1372u,1373u}) {
        ASSERT_TRUE(interop::make(data.data(),n,'E','L',0xffffffff));
        ASSERT_TRUE(interop::parse(data.data(),n,seq));EXPECT_EQ(seq,0xffffffff);
    }
    EXPECT_FALSE(interop::make(data.data(),13,'Q','L',0));
    EXPECT_FALSE(interop::make(data.data(),1374,'Q','L',0));
    EXPECT_FALSE(interop::parse(data.data(),13,seq));
    EXPECT_FALSE(interop::parse(data.data(),1374,seq));
    data[15]^=1;EXPECT_FALSE(interop::parse(data.data(),1373,seq));
}
TEST(InteropPacket, InvalidHeaderCannotBeEchoed) {
    uint8_t data[16]{};uint32_t seq;
    ASSERT_TRUE(interop::make(data,16,'Q','L',4));
    for(size_t i:{0u,8u,9u}){auto old=data[i];data[i]=0;EXPECT_FALSE(interop::parse(data,16,seq));data[i]=old;}
    EXPECT_FALSE(interop::make(data,16,'X','L',0));
}
