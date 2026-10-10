// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/platform.hpp>
#include <gtest/gtest.h>

using namespace neondoll;

// Helper to check that a platform context has all null pointers
static void expect_null_platform(const PlatformContext& p) {
    EXPECT_EQ(p.storage, nullptr);
    EXPECT_EQ(p.entropy, nullptr);
    EXPECT_EQ(p.clock, nullptr);
    EXPECT_EQ(p.network, nullptr);
    EXPECT_EQ(p.logger, nullptr);
    EXPECT_FALSE(p.initialized);
}

// Mock platform implementations for testing
class MockStorage : public Storage {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_result = 0;
    int deinit_result = 0;
    
    // Storage state
    std::map<std::string, std::vector<uint8_t>> stored_data;
    
    MockStorage() = default;
    ~MockStorage() override = default;

    int init() override { init_calls++; return init_result; }
    int deinit() override { deinit_calls++; return deinit_result; }
    
    ptrdiff_t read(const char* key, void* out, size_t max_size) override {
        if (!key) return -1;  // null key
        if (!out) return -1;  // null output
        
        auto it = stored_data.find(key);
        if (it == stored_data.end()) {
            return -1;  // key not found
        }
        
        const auto& data = it->second;
        if (max_size < data.size()) {
            return -2;  // buffer too small
        }
        
        // Copy data to output buffer
        memcpy(out, data.data(), data.size());
        return static_cast<ptrdiff_t>(data.size());
    }
    
    int write(const char* key, const void* in, size_t size) override {
        if (!key) return -1;  // null key
        if (!in && size > 0) return -1;  // null input with size > 0
        
        stored_data[key] = std::vector<uint8_t>(
            static_cast<const uint8_t*>(in),
            static_cast<const uint8_t*>(in) + size
        );
        return 0;
    }
};

class MockEntropy : public EntropySource {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_result = 0;
    int deinit_result = 0;
    int get_random_calls = 0;
    int get_random_result = 0;
    size_t last_len = 0;
    void* last_out = nullptr;

    MockEntropy() = default;
    ~MockEntropy() override = default;

    int init() override { init_calls++; return init_result; }
    int deinit() override { deinit_calls++; return deinit_result; }
    int get_random(void* out, size_t len) override {
        get_random_calls++;
        last_out = out;
        last_len = len;
        // Fill with deterministic data for testing
        if (out && len > 0) {
            memset(out, 0x42, len);
        }
        return get_random_result;
    }
};

class MockClock : public Clock {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_result = 0;
    int deinit_result = 0;
    uint64_t now_ms_result = 1234567890;

    MockClock() = default;
    ~MockClock() override = default;

    int init() override { init_calls++; return init_result; }
    int deinit() override { deinit_calls++; return deinit_result; }
    uint64_t now_ms() override { return now_ms_result; }
};

class MockNetwork : public NetworkAvailability {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_result = 0;
    int deinit_result = 0;
    int set_callback_calls = 0;
    int set_callback_result = 0;
    Callback last_cb = nullptr;
    void* last_arg = nullptr;

    MockNetwork() = default;
    ~MockNetwork() override = default;

    int init() override { init_calls++; return init_result; }
    int deinit() override { deinit_calls++; return deinit_result; }
    int set_callback(Callback cb, void* arg) override {
        set_callback_calls++;
        last_cb = cb;
        last_arg = arg;
        return set_callback_result;
    }
};

class MockLogger : public Logger {
public:
    int init_calls = 0;
    int deinit_calls = 0;
    int init_result = 0;
    int deinit_result = 0;
    int log_calls = 0;
    int log_result = 0;
    int last_level = 0;
    std::string last_tag;
    std::string last_msg;

    MockLogger() = default;
    ~MockLogger() override = default;

    int init() override { init_calls++; return init_result; }
    int deinit() override { deinit_calls++; return deinit_result; }
    int log(int level, const char* tag, const char* msg) override {
        log_calls++;
        last_level = level;
        last_tag = tag ? tag : "";
        last_msg = msg ? msg : "";
        return log_result;
    }
};

// Test fixture
class NeondollRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mocks
        storage = std::make_unique<MockStorage>();
        entropy = std::make_unique<MockEntropy>();
        clock = std::make_unique<MockClock>();
        network = std::make_unique<MockNetwork>();
        logger = std::make_unique<MockLogger>();
        
        // Set default success results
        storage->init_result = 0;
        storage->deinit_result = 0;
        
        entropy->init_result = 0;
        entropy->deinit_result = 0;
        entropy->get_random_result = 0;
        
        clock->init_result = 0;
        clock->deinit_result = 0;
        
        network->init_result = 0;
        network->deinit_result = 0;
        network->set_callback_result = 0;
        
        logger->init_result = 0;
        logger->deinit_result = 0;
        logger->log_result = 0;
        
        ctx.storage = storage.get();
        ctx.entropy = entropy.get();
        ctx.clock = clock.get();
        ctx.network = network.get();
        ctx.logger = logger.get();
        ctx.initialized = false;
    }

    void TearDown() override {
        // Ensure cleanup
        if (ctx.initialized) {
            neondoll_deinit(&ctx);
        }
    }

    std::unique_ptr<MockStorage> storage;
    std::unique_ptr<MockEntropy> entropy;
    std::unique_ptr<MockClock> clock;
    std::unique_ptr<MockNetwork> network;
    std::unique_ptr<MockLogger> logger;
    PlatformContext ctx;
};

TEST_F(NeondollRuntimeTest, InitSuccess) {
    EXPECT_EQ(neondoll_init(&ctx), 0);
    EXPECT_TRUE(ctx.initialized);
    
    // Check that init was called on all platforms
    EXPECT_EQ(storage->init_calls, 1);
    EXPECT_EQ(entropy->init_calls, 1);
    EXPECT_EQ(clock->init_calls, 1);
    EXPECT_EQ(network->init_calls, 1);
    EXPECT_EQ(logger->init_calls, 1);
}

TEST_F(NeondollRuntimeTest, DeinitSuccess) {
    ASSERT_EQ(neondoll_init(&ctx), 0);
    EXPECT_EQ(neondoll_deinit(&ctx), 0);
    EXPECT_FALSE(ctx.initialized);
    
    // Check that deinit was called on all platforms
    EXPECT_EQ(storage->deinit_calls, 1);
    EXPECT_EQ(entropy->deinit_calls, 1);
    EXPECT_EQ(clock->deinit_calls, 1);
    EXPECT_EQ(network->deinit_calls, 1);
    EXPECT_EQ(logger->deinit_calls, 1);
}

TEST_F(NeondollRuntimeTest, DoubleInitPrevention) {
    ASSERT_EQ(neondoll_init(&ctx), 0);
    EXPECT_EQ(neondoll_init(&ctx), -3); // Already initialized
}

TEST_F(NeondollRuntimeTest, DeinitWithoutInit) {
    EXPECT_EQ(neondoll_deinit(&ctx), -2); // Not initialized
}

TEST_F(NeondollRuntimeTest, NullContext) {
    EXPECT_EQ(neondoll_init(nullptr), -1);
    EXPECT_EQ(neondoll_deinit(nullptr), -1);
}

TEST_F(NeondollRuntimeTest, NullPlatformPointers) {
    ctx.storage = nullptr;
    EXPECT_EQ(neondoll_init(&ctx), -2);
    
    ctx.storage = storage.get();
    ctx.entropy = nullptr;
    EXPECT_EQ(neondoll_init(&ctx), -2);
    
    ctx.entropy = entropy.get();
    ctx.clock = nullptr;
    EXPECT_EQ(neondoll_init(&ctx), -2);
    
    ctx.clock = clock.get();
    ctx.network = nullptr;
    EXPECT_EQ(neondoll_init(&ctx), -2);
    
    ctx.network = network.get();
    ctx.logger = nullptr;
    EXPECT_EQ(neondoll_init(&ctx), -2);
}

TEST_F(NeondollRuntimeTest, StorageInitFailureRollback) {
    storage->init_result = -1; // Fail
    
    EXPECT_EQ(neondoll_init(&ctx), -4); // Storage init failed
    EXPECT_FALSE(ctx.initialized);
    
    // Storage init should have been called
    EXPECT_EQ(storage->init_calls, 1);
    // Storage deinit should NOT have been called (init failed before completion)
    EXPECT_EQ(storage->deinit_calls, 0);
    // Other platforms should not have been initialized
    EXPECT_EQ(entropy->init_calls, 0);
    EXPECT_EQ(clock->init_calls, 0);
    EXPECT_EQ(network->init_calls, 0);
    EXPECT_EQ(logger->init_calls, 0);
}

TEST_F(NeondollRuntimeTest, EntropyInitFailureRollback) {
    storage->init_result = 0;  // Success
    entropy->init_result = -1; // Fail
    
    EXPECT_EQ(neondoll_init(&ctx), -5); // Entropy init failed
    EXPECT_FALSE(ctx.initialized);
    
    // Storage should be initialized and deinitialized (rollback)
    EXPECT_EQ(storage->init_calls, 1);
    EXPECT_EQ(storage->deinit_calls, 1);
    // Entropy init should have been called
    EXPECT_EQ(entropy->init_calls, 1);
    // Other platforms should not have been initialized
    EXPECT_EQ(clock->init_calls, 0);
    EXPECT_EQ(network->init_calls, 0);
    EXPECT_EQ(logger->init_calls, 0);
}

TEST_F(NeondollRuntimeTest, ClockInitFailureRollback) {
    storage->init_result = 0;
    entropy->init_result = 0;
    clock->init_result = -1; // Fail
    
    EXPECT_EQ(neondoll_init(&ctx), -6); // Clock init failed
    EXPECT_FALSE(ctx.initialized);
    
    // Storage and entropy should be initialized and deinitialized (rollback)
    EXPECT_EQ(storage->init_calls, 1);
    EXPECT_EQ(storage->deinit_calls, 1);
    EXPECT_EQ(entropy->init_calls, 1);
    EXPECT_EQ(entropy->deinit_calls, 1);
    // Clock init should have been called
    EXPECT_EQ(clock->init_calls, 1);
    // Other platforms should not have been initialized
    EXPECT_EQ(network->init_calls, 0);
    EXPECT_EQ(logger->init_calls, 0);
}

TEST_F(NeondollRuntimeTest, NetworkInitFailureRollback) {
    storage->init_result = 0;
    entropy->init_result = 0;
    clock->init_result = 0;
    network->init_result = -1; // Fail
    
    EXPECT_EQ(neondoll_init(&ctx), -7); // Network init failed
    EXPECT_FALSE(ctx.initialized);
    
    // Storage, entropy, clock should be initialized and deinitialized (rollback)
    EXPECT_EQ(storage->init_calls, 1);
    EXPECT_EQ(storage->deinit_calls, 1);
    EXPECT_EQ(entropy->init_calls, 1);
    EXPECT_EQ(entropy->deinit_calls, 1);
    EXPECT_EQ(clock->init_calls, 1);
    EXPECT_EQ(clock->deinit_calls, 1);
    // Network init should have been called
    EXPECT_EQ(network->init_calls, 1);
    // Logger should not have been initialized
    EXPECT_EQ(logger->init_calls, 0);
}

TEST_F(NeondollRuntimeTest, LoggerInitFailureRollback) {
    storage->init_result = 0;
    entropy->init_result = 0;
    clock->init_result = 0;
    network->init_result = 0;
    logger->init_result = -1; // Fail
    
    EXPECT_EQ(neondoll_init(&ctx), -8); // Logger init failed
    EXPECT_FALSE(ctx.initialized);
    
    // All platforms should be initialized and deinitialized (rollback)
    EXPECT_EQ(storage->init_calls, 1);
    EXPECT_EQ(storage->deinit_calls, 1);
    EXPECT_EQ(entropy->init_calls, 1);
    EXPECT_EQ(entropy->deinit_calls, 1);
    EXPECT_EQ(clock->init_calls, 1);
    EXPECT_EQ(clock->deinit_calls, 1);
    EXPECT_EQ(network->init_calls, 1);
    EXPECT_EQ(network->deinit_calls, 1);
    // Logger init should have been called
    EXPECT_EQ(logger->init_calls, 1);
}

// Test two runtime instances
TEST_F(NeondollRuntimeTest, TwoIndependentInstances) {
    PlatformContext ctx1, ctx2;
    ctx1.storage = storage.get();
    ctx1.entropy = entropy.get();
    ctx1.clock = clock.get();
    ctx1.network = network.get();
    ctx1.logger = logger.get();
    
    // Create second set of mocks for ctx2
    auto storage2 = std::make_unique<MockStorage>();
    auto entropy2 = std::make_unique<MockEntropy>();
    auto clock2 = std::make_unique<MockClock>();
    auto network2 = std::make_unique<MockNetwork>();
    auto logger2 = std::make_unique<MockLogger>();
    
    // Set default success results for second set
    storage2->init_result = 0;
    storage2->deinit_result = 0;
    entropy2->init_result = 0;
    entropy2->deinit_result = 0;
    entropy2->get_random_result = 0;
    clock2->init_result = 0;
    clock2->deinit_result = 0;
    network2->init_result = 0;
    network2->deinit_result = 0;
    network2->set_callback_result = 0;
    logger2->init_result = 0;
    logger2->deinit_result = 0;
    logger2->log_result = 0;
    
    ctx2.storage = storage2.get();
    ctx2.entropy = entropy2.get();
    ctx2.clock = clock2.get();
    ctx2.network = network2.get();
    ctx2.logger = logger2.get();
    
    // Initialize both
    EXPECT_EQ(neondoll_init(&ctx1), 0);
    EXPECT_EQ(neondoll_init(&ctx2), 0);
    EXPECT_TRUE(ctx1.initialized);
    EXPECT_TRUE(ctx2.initialized);
    
    // Deinitialize in reverse order
    EXPECT_EQ(neondoll_deinit(&ctx2), 0);
    EXPECT_EQ(neondoll_deinit(&ctx1), 0);
    EXPECT_FALSE(ctx1.initialized);
    EXPECT_FALSE(ctx2.initialized);
}

// Test mismatched deinit context
TEST_F(NeondollRuntimeTest, MismatchedDeinitContext) {
    PlatformContext ctx1, ctx2;
    ctx1.storage = storage.get();
    ctx1.entropy = entropy.get();
    ctx1.clock = clock.get();
    ctx1.network = network.get();
    ctx1.logger = logger.get();
    
    ctx2.storage = storage.get(); // Same storage
    ctx2.entropy = entropy.get(); // Same entropy
    ctx2.clock = clock.get();     // Same clock
    ctx2.network = network.get(); // Same network
    ctx2.logger = logger.get();   // Same logger
    
    // Initialize first context
    EXPECT_EQ(neondoll_init(&ctx1), 0);
    EXPECT_TRUE(ctx1.initialized);
    
    // Try to deinitialize with second context (should fail - not initialized)
    EXPECT_EQ(neondoll_deinit(&ctx2), -2); // Not initialized
    EXPECT_TRUE(ctx1.initialized); // First should still be initialized
    
    // Properly deinitialize first context
    EXPECT_EQ(neondoll_deinit(&ctx1), 0);
    EXPECT_FALSE(ctx1.initialized);
}

// Test repeated init/deinit cycles
TEST_F(NeondollRuntimeTest, RepeatedInitDeinit) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(neondoll_init(&ctx), 0);
        EXPECT_TRUE(ctx.initialized);
        EXPECT_EQ(neondoll_deinit(&ctx), 0);
        EXPECT_FALSE(ctx.initialized);
    }
}

// Test storage binary interface
TEST_F(NeondollRuntimeTest, StorageBinaryInterface) {
    ASSERT_EQ(neondoll_init(&ctx), 0);
    
    // Test read on empty storage (key not found)
    char buf[10];
    EXPECT_EQ(storage->read("nonexistent", buf, sizeof(buf)), -1);
    
    // Test write
    const char* test_key = "test";
    const char test_val[] = "hello";
    EXPECT_EQ(storage->write(test_key, test_val, sizeof(test_val)-1), 0);
    
    // Test read with sufficient buffer
    EXPECT_EQ(storage->read(test_key, buf, sizeof(buf)), 5);
    EXPECT_EQ(memcmp(buf, test_val, 5), 0);
    
    // Test read with insufficient buffer (buffer too small)
    EXPECT_EQ(storage->read(test_key, buf, 3), -2);
    
    // Test read with null key
    EXPECT_EQ(storage->read(nullptr, buf, sizeof(buf)), -1);
    
    // Test read with null output
    EXPECT_EQ(storage->read(test_key, nullptr, sizeof(buf)), -1);
    
    neondoll_deinit(&ctx);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}