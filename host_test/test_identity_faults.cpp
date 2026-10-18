#include <gtest/gtest.h>
#include <neondoll/platform.hpp>
#include <neondoll/identity_types.hpp>
#include <neondoll/identity_storage.hpp>
#include <cstring>
#include <limits>

namespace {
struct Storage : neondoll::Storage {
    bool exists = false;
    int reads = 0, writes = 0, error = 0;
    std::string record;
    int init() override { return 0; }
    int deinit() override { return 0; }
    std::ptrdiff_t read(const char*, void* out, size_t max) override {
        ++reads;
        if (error) return error;
        if (!exists) return -1;
        if (record.size() > max) return -2;
        std::memcpy(out, record.data(), record.size());
        return static_cast<std::ptrdiff_t>(record.size());
    }
    int write(const char*, const void* in, size_t size) override {
        ++writes; exists = true; record.assign(static_cast<const char*>(in), size); return 0;
    }
};
struct Entropy : neondoll::EntropySource {
    int calls = 0, result = 0;
    int init() override { return 0; }
    int deinit() override { return 0; }
    int get_random(void* out, size_t size) override {
        ++calls;
        std::memset(out, calls, result ? 1 : size);
        return result;
    }
};
struct Clock : neondoll::Clock {
    int init() override { return 0; } int deinit() override { return 0; }
    uint64_t now_ms() override { return 0; }
};
struct Network : neondoll::NetworkAvailability {
    int init() override { return 0; } int deinit() override { return 0; }
    int set_callback(Callback, void*) override { return 0; }
};
struct Logger : neondoll::Logger {
    int init() override { return 0; } int deinit() override { return 0; }
    int log(int, const char*, const char*) override { return 0; }
};
struct IdentityFaults : testing::Test {
    Storage storage; Entropy entropy; Clock clock; Network network; Logger logger;
    neondoll::PlatformContext ctx;
    IdentityFaults() {
        ctx.storage=&storage; ctx.entropy=&entropy; ctx.clock=&clock;
        ctx.network=&network; ctx.logger=&logger;
    }
    void TearDown() override { if (ctx.initialized) neondoll_deinit(&ctx); }
};
TEST_F(IdentityFaults, RebootsNeverRegenerateOrRewriteIdentity) {
    ASSERT_EQ(neondoll_init(&ctx), 0);
    const auto id = ctx.identity_state.identity.body_id;
    for (int i=0; i<100; ++i) {
        ASSERT_EQ(neondoll_deinit(&ctx), 0);
        ctx.identity_state = {};
        ASSERT_EQ(neondoll_init(&ctx), 0);
        EXPECT_EQ(ctx.identity_state.identity.body_id, id);
    }
    EXPECT_EQ(entropy.calls, 1); EXPECT_EQ(storage.writes, 1);
}
TEST_F(IdentityFaults, ShortOrFailedEntropyDoesNotPersist) {
    for (int result : {1, 15, 16, -1}) {
        entropy.result = result;
        EXPECT_EQ(neondoll_init(&ctx), -10);
        EXPECT_FALSE(ctx.initialized);
        EXPECT_EQ(storage.writes, 0);
    }
}
TEST_F(IdentityFaults, LegacyUpgradePreservesIDAndRunsOnce) {
    storage.exists = true;
    storage.record = "v1:0123456789abcdef0123456789abcdef:42:99";
    ASSERT_EQ(neondoll_init(&ctx), 0);
    EXPECT_EQ(ctx.identity_state.identity.body_id, "0123456789abcdef0123456789abcdef");
    EXPECT_EQ(ctx.identity_state.identity.counter, 42u);
    EXPECT_EQ(storage.record.substr(0,3), "v2:");
    EXPECT_EQ(entropy.calls, 0);
    EXPECT_EQ(storage.writes, 1);
    neondoll_deinit(&ctx);
    ASSERT_EQ(neondoll_init(&ctx), 0);
    EXPECT_EQ(storage.writes, 1);
}
TEST_F(IdentityFaults, InvalidExistingStateNeverCreatesReplacement) {
    storage.exists = true;
    for (const std::string& record : {std::string(), std::string("garbage"),
         std::string("v9:0123456789abcdef0123456789abcdef:0:0"),
         std::string("v1:0123456789abcdef0123456789abcdef:18446744073709551616:0"),
         std::string(1025, 'x')}) {
        storage.record = record;
        EXPECT_EQ(neondoll_init(&ctx), -10);
        EXPECT_EQ(storage.record, record);
        EXPECT_EQ(neondoll_get_identity(&ctx), nullptr);
    }
    for (int error : {-2, -3, -4}) {
        storage.error = error;
        EXPECT_EQ(neondoll_init(&ctx), -10);
    }
    EXPECT_EQ(entropy.calls, 0); EXPECT_EQ(storage.writes, 0);
}
TEST_F(IdentityFaults, RecordRoundtripAndEverySingleByteCorruption) {
    neondoll::IdentityState state{};
    ASSERT_EQ(neondoll::new_identity_state(&entropy,"Name:with delimiter",{"embedded","esp32","xtensa"},state),0);
    state.identity.counter=std::numeric_limits<uint64_t>::max();
    const auto record=neondoll::format_identity(state);
    neondoll::IdentityState loaded{};
    ASSERT_TRUE(neondoll::parse_identity(record, loaded));
    EXPECT_EQ(loaded.identity.name,state.identity.name);
    EXPECT_EQ(loaded.identity.meta.platform,"esp32");
    EXPECT_EQ(loaded.identity.meta.arch,"xtensa");
    EXPECT_EQ(loaded.identity.meta.implementation,"embedded");
    EXPECT_EQ(loaded.identity.counter,state.identity.counter);
    for (size_t i=0;i<record.size();++i) {
        auto corrupt=record; corrupt[i]^=1;
        EXPECT_FALSE(neondoll::parse_identity(corrupt, loaded)) << i;
    }
    for (size_t i=0;i<record.size();++i)
        EXPECT_FALSE(neondoll::parse_identity(record.substr(0,i),loaded)) << i;
    EXPECT_EQ(loaded.identity.body_id,state.identity.body_id);
}
TEST_F(IdentityFaults, DistinctLoadErrorsAndFieldLimits) {
    neondoll::IdentityState state{};
    EXPECT_EQ(neondoll::load_identity(&storage,"key",state),-3);
    storage.exists=true; storage.record="v3:future";
    EXPECT_EQ(neondoll::load_identity(&storage,"key",state),-5);
    storage.record="";
    EXPECT_EQ(neondoll::load_identity(&storage,"key",state),-4);
    storage.error=-2;
    EXPECT_EQ(neondoll::load_identity(&storage,"key",state),-7);
    storage.error=-3;
    EXPECT_EQ(neondoll::load_identity(&storage,"key",state),-6);
    ASSERT_EQ(neondoll::new_identity_state(&entropy,std::string(65,'n'),{},state),0);
    EXPECT_EQ(neondoll::save_identity(&storage,"key",state),-4);
    EXPECT_EQ(storage.writes,0);
}
TEST_F(IdentityFaults, GenerationReplacesOutputAndPreservesItOnFailure) {
    std::string id="old";
    ASSERT_EQ(neondoll::generate_body_id(&entropy,id),0);
    EXPECT_EQ(id.size(),32u);
    const auto saved=id; entropy.result=-1;
    EXPECT_NE(neondoll::generate_body_id(&entropy,id),0);
    EXPECT_EQ(id,saved);
}
} // namespace
