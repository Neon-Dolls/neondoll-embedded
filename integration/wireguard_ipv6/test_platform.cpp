#include <gtest/gtest.h>
extern "C" {
int wireguard_platform_init(void);void wireguard_platform_deinit(void);
void wireguard_random_bytes(void *,size_t);
void nd_platform_fault(int);int nd_platform_cleared(void);int nd_platform_seeds(void);
}
class PlatformTest : public testing::Test {
    void SetUp() override {wireguard_platform_deinit();nd_platform_fault(0);}
    void TearDown() override {wireguard_platform_deinit();EXPECT_TRUE(nd_platform_cleared());}
};
TEST_F(PlatformTest, SuccessfulInitIsIdempotentAndTeardownWipesState) {
    int seeds=nd_platform_seeds();ASSERT_EQ(wireguard_platform_init(),0);ASSERT_EQ(wireguard_platform_init(),0);
    EXPECT_EQ(nd_platform_seeds(),seeds+1);unsigned char out[32];wireguard_random_bytes(out,sizeof(out));
    wireguard_platform_deinit();EXPECT_TRUE(nd_platform_cleared());wireguard_platform_deinit();EXPECT_TRUE(nd_platform_cleared());
}
TEST_F(PlatformTest, EntropyRegistrationFailureWipesPartialContextsAndRecovers) {
    nd_platform_fault(1);EXPECT_NE(wireguard_platform_init(),0);EXPECT_TRUE(nd_platform_cleared());
    nd_platform_fault(0);EXPECT_EQ(wireguard_platform_init(),0);
}
TEST_F(PlatformTest, SeedFailureWipesPartialContextsAndRecovers) {
    nd_platform_fault(2);EXPECT_NE(wireguard_platform_init(),0);EXPECT_TRUE(nd_platform_cleared());
    nd_platform_fault(0);EXPECT_EQ(wireguard_platform_init(),0);
}
TEST_F(PlatformTest, RandomFailureCannotSupplyInvalidKeyMaterial) {
    ASSERT_EQ(wireguard_platform_init(),0);nd_platform_fault(3);unsigned char out[32];
    EXPECT_DEATH(wireguard_random_bytes(out,sizeof(out)),"");
}
TEST_F(PlatformTest, UninitializedRandomFailsClosed) {
    unsigned char out[32];EXPECT_DEATH(wireguard_random_bytes(out,sizeof(out)),"");
}
TEST_F(PlatformTest, RepeatedCyclesAlwaysEraseOwnedContexts) {
    for(int i=0;i<500;++i){ASSERT_EQ(wireguard_platform_init(),0);wireguard_platform_deinit();ASSERT_TRUE(nd_platform_cleared());}
}
