// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <cstring>
#include <neondoll/platform.hpp>

namespace {

class MockStorage : public neondoll::Storage {
public:
    MockStorage() : init_calls(0), deinit_calls(0), read_calls(0), write_calls(0),
                       init_return(0), deinit_return(0), read_return(0), write_return(0) {}
    int init() override { ++init_calls; return init_return; }
    int deinit() override { ++deinit_calls; return deinit_return; }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override {
        ++read_calls;
        if (read_return < 0) {
            return read_return;
        }
        auto it = storage_.find(key);
        if (it == storage_.end()) {
            return -1;
        }
        if (it->second.size() > max_size) {
            return -2; // buffer too small
        }
        memcpy(out, it->second.data(), it->second.size());
        return static_cast<std::ptrdiff_t>(it->second.size());
    }
    int write(const char* key, const void* in, size_t size) override {
        ++write_calls;
        if (write_return < 0) {
            return write_return;
        }
        storage_[key] = std::string(static_cast<const char*>(in), size);
        return 0; // success
    }

    void set_fail_init(bool fail) { fail_init_ = fail; }
    void set_fail_random(bool fail) { fail_random_ = fail; }

public: // Test accessors
    int init_calls;
    int deinit_calls;
    int read_calls;
    int write_calls;
    int init_return;
    int deinit_return;
    std::ptrdiff_t read_return;
    int write_return;
    bool fail_init_;
    bool fail_random_;
    std::map<std::string, std::string> storage_;
};
class MockEntropy : public neondoll::EntropySource {
public:
    MockEntropy() : init_calls(0), deinit_calls(0), get_random_calls(0),
                       init_return(0), deinit_return(0), get_random_return(0) {}
    int init() override { ++init_calls; return init_return; }
    int deinit() override { ++deinit_calls; return deinit_return; }
    int get_random(void* out, size_t len) override {
        ++get_random_calls;
        if (get_random_return < 0) {
            return get_random_return;
        }
        // Fill buffer with a fixed pattern (0xAA) for testing
        memset(out, 0xAA, len);
        return 0;
    }

    void set_fail_init(bool fail) { fail_init_ = fail; }
    void set_fail_random(bool fail) { fail_random_ = fail; }

public: // Test accessors
    int init_calls;
    int deinit_calls;
    int get_random_calls;
    int init_return;
    int deinit_return;
    int get_random_return;
    bool fail_init_;
    bool fail_random_;
};
class MockClock : public neondoll::Clock {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_return = 0;
    int deinit_return = 0;
    uint64_t now_ms_return = 0;

    int init() override { ++init_calls; return init_return; }
    int deinit() override { ++deinit_calls; return deinit_return; }
    uint64_t now_ms() override { return now_ms_return; }
};

class MockNetwork : public neondoll::NetworkAvailability {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_return = 0;
    int deinit_return = 0;
    int set_callback_return = -1;

    int init() override { ++init_calls; return init_return; }
    int deinit() override { ++deinit_calls; return deinit_return; }
    int set_callback(Callback, void*) override { return set_callback_return; }
};

class MockLogger : public neondoll::Logger {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_return = 0;
    int deinit_return = 0;
    int log_return = -1;

    int init() override { ++init_calls; return init_return; }
    int deinit() override { ++deinit_calls; return deinit_return; }
    int log(int, const char*, const char*) override { return log_return; }
};

TEST(NeondollLifecycleTest, BasicInitDeinitSuccess) {
    neondoll::PlatformContext ctx;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    EXPECT_EQ(neondoll_init(&ctx), 0);
    EXPECT_TRUE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 1);
    EXPECT_EQ(entropy.init_calls, 1);
    EXPECT_EQ(clock.init_calls, 1);
    EXPECT_EQ(network.init_calls, 1);
    EXPECT_EQ(logger.init_calls, 1);

    EXPECT_EQ(neondoll_deinit(&ctx), 0);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.deinit_calls, 1);
    EXPECT_EQ(entropy.deinit_calls, 1);
    EXPECT_EQ(clock.deinit_calls, 1);
    EXPECT_EQ(network.deinit_calls, 1);
    EXPECT_EQ(logger.deinit_calls, 1);
}

TEST(NeondollLifecycleTest, TwoRuntimeInstances) {
    neondoll::PlatformContext ctx1, ctx2;
    MockStorage storage1, storage2;
    MockEntropy entropy1, entropy2;
    MockClock clock1, clock2;
    MockNetwork network1, network2;
    MockLogger logger1, logger2;

    ctx1.storage = &storage1; ctx1.entropy = &entropy1; ctx1.clock = &clock1; ctx1.network = &network1; ctx1.logger = &logger1;
    ctx2.storage = &storage2; ctx2.entropy = &entropy2; ctx2.clock = &clock2; ctx2.network = &network2; ctx2.logger = &logger2;

    // Initialize first
    EXPECT_EQ(neondoll_init(&ctx1), 0);
    EXPECT_TRUE(ctx1.initialized);
    EXPECT_FALSE(ctx2.initialized);

    // Initialize second
    EXPECT_EQ(neondoll_init(&ctx2), 0);
    EXPECT_TRUE(ctx1.initialized);
    EXPECT_TRUE(ctx2.initialized);

    // Deinitialize first
    EXPECT_EQ(neondoll_deinit(&ctx1), 0);
    EXPECT_FALSE(ctx1.initialized);
    EXPECT_TRUE(ctx2.initialized);

    // Deinitialize second
    EXPECT_EQ(neondoll_deinit(&ctx2), 0);
    EXPECT_FALSE(ctx1.initialized);
    EXPECT_FALSE(ctx2.initialized);
}

TEST(NeondollLifecycleTest, MismatchedDeinitContextFails) {
    neondoll::PlatformContext ctx1, ctx2;
    MockStorage storage1, storage2;
    MockEntropy entropy1, entropy2;
    MockClock clock1, clock2;
    MockNetwork network1, network2;
    MockLogger logger1, logger2;

    ctx1.storage = &storage1; ctx1.entropy = &entropy1; ctx1.clock = &clock1; ctx1.network = &network1; ctx1.logger = &logger1;
    ctx2.storage = &storage2; ctx2.entropy = &entropy2; ctx2.clock = &clock2; ctx2.network = &network2; ctx2.logger = &logger2;

    EXPECT_EQ(neondoll_init(&ctx1), 0);
    EXPECT_TRUE(ctx1.initialized);

    // Deinitialize with wrong context should fail (not initialized)
    EXPECT_EQ(neondoll_deinit(&ctx2), -2); // not initialized
    EXPECT_TRUE(ctx1.initialized); // ctx1 still initialized

    // Proper deinit
    EXPECT_EQ(neondoll_deinit(&ctx1), 0);
    EXPECT_FALSE(ctx1.initialized);
}

TEST(NeondollLifecycleTest, RepeatedInitDeinit) {
    neondoll::PlatformContext ctx;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(neondoll_init(&ctx), 0) << "Iteration " << i;
        EXPECT_TRUE(ctx.initialized);
        EXPECT_EQ(storage.init_calls, i+1);
        EXPECT_EQ(entropy.init_calls, i+1);
        EXPECT_EQ(clock.init_calls, i+1);
        EXPECT_EQ(network.init_calls, i+1);
        EXPECT_EQ(logger.init_calls, i+1);

        EXPECT_EQ(neondoll_deinit(&ctx), 0) << "Iteration " << i;
        EXPECT_FALSE(ctx.initialized);
        EXPECT_EQ(storage.deinit_calls, i+1);
        EXPECT_EQ(entropy.deinit_calls, i+1);
        EXPECT_EQ(clock.deinit_calls, i+1);
        EXPECT_EQ(network.deinit_calls, i+1);
        EXPECT_EQ(logger.deinit_calls, i+1);
    }
}

TEST(NeondollLifecycleTest, PartialInitializationFailure) {
    neondoll::PlatformContext ctx;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    // Storage fails
    storage.init_return = -1;
    EXPECT_EQ(neondoll_init(&ctx), -4);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 1);
    EXPECT_EQ(storage.deinit_calls, 0); // not deinitialized because init failed before storage init? Actually storage init is called and fails, so we don't call deinit on storage in the init function because we return immediately. So storage deinit should be 0.
    EXPECT_EQ(entropy.init_calls, 0);
    EXPECT_EQ(clock.init_calls, 0);
    EXPECT_EQ(network.init_calls, 0);
    EXPECT_EQ(logger.init_calls, 0);

    // Reset storage, entropy fails
    storage.init_return = 0;
    entropy.init_return = -1;
    EXPECT_EQ(neondoll_init(&ctx), -5);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 2);
    EXPECT_EQ(storage.deinit_calls, 1); // storage deinit called on entropy failure
    EXPECT_EQ(entropy.init_calls, 1);
    EXPECT_EQ(clock.init_calls, 0);
    EXPECT_EQ(network.init_calls, 0);
    EXPECT_EQ(logger.init_calls, 0);

    // Reset entropy, clock fails
    entropy.init_return = 0;
    clock.init_return = -1;
    EXPECT_EQ(neondoll_init(&ctx), -6);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 3);
    EXPECT_EQ(storage.deinit_calls, 2);
    EXPECT_EQ(entropy.init_calls, 2);
    EXPECT_EQ(entropy.deinit_calls, 1);
    EXPECT_EQ(clock.init_calls, 1);
    EXPECT_EQ(clock.deinit_calls, 0);
    EXPECT_EQ(network.init_calls, 0);
    EXPECT_EQ(logger.init_calls, 0);

    // Reset clock, network fails
    clock.init_return = 0;
    network.init_return = -1;
    EXPECT_EQ(neondoll_init(&ctx), -7);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 4);
    EXPECT_EQ(storage.deinit_calls, 3);
    EXPECT_EQ(entropy.init_calls, 3);
    EXPECT_EQ(entropy.deinit_calls, 2);
    EXPECT_EQ(clock.init_calls, 2);
    EXPECT_EQ(clock.deinit_calls, 1);
    EXPECT_EQ(network.init_calls, 1);
    EXPECT_EQ(network.deinit_calls, 0);
    EXPECT_EQ(logger.init_calls, 0);

    // Reset network, logger fails
    network.init_return = 0;
    logger.init_return = -1;
    EXPECT_EQ(neondoll_init(&ctx), -8);
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(storage.init_calls, 5);
    EXPECT_EQ(storage.deinit_calls, 4);
    EXPECT_EQ(entropy.init_calls, 4);
    EXPECT_EQ(entropy.deinit_calls, 3);
    EXPECT_EQ(clock.init_calls, 3);
    EXPECT_EQ(clock.deinit_calls, 2);
    EXPECT_EQ(network.init_calls, 2);
    EXPECT_EQ(network.deinit_calls, 1);
    EXPECT_EQ(logger.init_calls, 1);
    EXPECT_EQ(logger.deinit_calls, 0);
}

TEST(NeondollLifecycleTest, NullContext) {
    EXPECT_EQ(neondoll_init(nullptr), -1);
    EXPECT_EQ(neondoll_deinit(nullptr), -1);
}

TEST(NeondollLifecycleTest, NullPlatformPointer) {
    neondoll::PlatformContext ctx;
    ctx.storage = nullptr;
    ctx.entropy = nullptr;
    ctx.clock = nullptr;
    ctx.network = nullptr;
    ctx.logger = nullptr;

    EXPECT_EQ(neondoll_init(&ctx), -2);
    EXPECT_FALSE(ctx.initialized);
}

TEST(NeondollLifecycleTest, AlreadyInitialized) {
    neondoll::PlatformContext ctx;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    EXPECT_EQ(neondoll_init(&ctx), 0);
    EXPECT_TRUE(ctx.initialized);

    EXPECT_EQ(neondoll_init(&ctx), -3);
    EXPECT_TRUE(ctx.initialized);

    EXPECT_EQ(neondoll_deinit(&ctx), 0);
    EXPECT_FALSE(ctx.initialized);
}



TEST(NeondollLifecycleTest, FirstBootPersistDeinitRestartIdenticalBodyID) {
    neondoll::PlatformContext ctx1, ctx2;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    // Use the same storage instance for both contexts to simulate persistence
    ctx1.storage = &storage; ctx1.entropy = &entropy; ctx1.clock = &clock; ctx1.network = &network; ctx1.logger = &logger;
    ctx2.storage = &storage; ctx2.entropy = &entropy; ctx2.clock = &clock; ctx2.network = &network; ctx2.logger = &logger;

    // First boot - create and persist identity
    EXPECT_EQ(neondoll_init(&ctx1), 0);
    EXPECT_TRUE(ctx1.initialized);
    const neondoll::IdentityState* identity1 = neondoll_get_identity(&ctx1);
    ASSERT_NE(identity1, nullptr);
    std::string first_body_id = identity1->identity.body_id;
    EXPECT_FALSE(first_body_id.empty());

    // Persist by deiniting (which should save via neondoll_init's logic on first boot)
    EXPECT_EQ(neondoll_deinit(&ctx1), 0);
    EXPECT_FALSE(ctx1.initialized);

    // Second boot - restart and load identical identity
    EXPECT_EQ(neondoll_init(&ctx2), 0);
    EXPECT_TRUE(ctx2.initialized);
    const neondoll::IdentityState* identity2 = neondoll_get_identity(&ctx2);
    ASSERT_NE(identity2, nullptr);
    std::string second_body_id = identity2->identity.body_id;
    EXPECT_FALSE(second_body_id.empty());
    EXPECT_EQ(first_body_id, second_body_id) << "Body ID should persist across deinit/init cycle";

    // Clean up
    EXPECT_EQ(neondoll_deinit(&ctx2), 0);
    EXPECT_FALSE(ctx2.initialized);
}

TEST(NeondollLifecycleTest, CorruptRecordIsRejectedWithoutReplacement) {
    neondoll::PlatformContext ctx;
    MockStorage storage;
    MockEntropy entropy;
    MockClock clock;
    MockNetwork network;
    MockLogger logger;

    ctx.storage = &storage; ctx.entropy = &entropy; ctx.clock = &clock; ctx.network = &network; ctx.logger = &logger;

    // First boot - create identity
    EXPECT_EQ(neondoll_init(&ctx), 0);
    EXPECT_TRUE(ctx.initialized);
    const neondoll::IdentityState* identity1 = neondoll_get_identity(&ctx);
    ASSERT_NE(identity1, nullptr);
    std::string first_body_id = identity1->identity.body_id;
    EXPECT_FALSE(first_body_id.empty());

    // Corrupt the persisted identity by writing invalid data
    storage.read_return = 0;   // allow reading from the storage map
    storage.read_return = 21;  // size of corrupt data without quotes
    storage.storage_["neondoll/identity"] = "invalid!@#$%^&*()data";

    // Deinit and restart with corrupted data
    EXPECT_EQ(neondoll_deinit(&ctx), 0);
    EXPECT_FALSE(ctx.initialized);

    // Second boot should fail to load corrupted identity (reject without replacement)
    EXPECT_EQ(neondoll_init(&ctx), -10); // failed to load or create identity
    EXPECT_FALSE(ctx.initialized);

    // Third boot should also fail to load corrupted identity (still corrupt, no replacement)
    EXPECT_EQ(neondoll_init(&ctx), -10);
    EXPECT_FALSE(ctx.initialized);

    // Now, if we remove the corrupt record (simulate missing key), we should get a new identity
    storage.storage_.erase("neondoll/identity");
    EXPECT_EQ(neondoll_deinit(&ctx), -2);   // deinit uninitialized context
    EXPECT_FALSE(ctx.initialized);
    EXPECT_EQ(neondoll_init(&ctx), 0);
    EXPECT_TRUE(ctx.initialized);
    const neondoll::IdentityState* identity3 = neondoll_get_identity(&ctx);
    ASSERT_NE(identity3, nullptr);
    std::string third_body_id = identity3->identity.body_id;
    EXPECT_FALSE(third_body_id.empty());
    EXPECT_EQ(neondoll_deinit(&ctx), 0);
    EXPECT_FALSE(ctx.initialized);
}


} // namespace
