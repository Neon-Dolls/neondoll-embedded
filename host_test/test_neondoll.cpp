#include <neondoll/platform.hpp>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

// Mock implementations for each interface with failure simulation
class MockStorage : public neondoll::Storage {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init_result = 0;
    int deinit_result = 0;
    int read_result = -1; // default: not found
    int write_result = -1; // default: error
    size_t last_read_size = 0;
    std::ptrdiff_t last_read_bytes = -1;
    size_t last_write_size = 0;

    int init() override { 
        init_called = true; 
        return init_result; 
    }
    int deinit() override { 
        deinit_called = true; 
        return deinit_result; 
    }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override {
        last_read_size = max_size;
        if (read_result >= 0) {
            // Simulate successful read: fill with pattern
            std::ptrdiff_t bytes_to_copy = std::min<std::ptrdiff_t>(read_result, static_cast<std::ptrdiff_t>(max_size));
            for (size_t i = 0; i < static_cast<size_t>(bytes_to_copy); ++i) {
                static_cast<char*>(out)[i] = static_cast<char>('A' + (i % 26));
            }
            last_read_bytes = bytes_to_copy;
        }
        return read_result;
    }
    int write(const char* key, const void* in, size_t size) override {
        last_write_size = size;
        return write_result;
    }
};

class MockEntropySource : public neondoll::EntropySource {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init_result = 0;
    int deinit_result = 0;
    int get_random_result = -1;

    int init() override { 
        init_called = true; 
        return init_result; 
    }
    int deinit() override { 
        deinit_called = true; 
        return deinit_result; 
    }
    int get_random(void* out, size_t len) override { 
        if (get_random_result >= 0) {
            // Fill with pattern
            for (size_t i = 0; i < len; ++i) {
                static_cast<uint8_t*>(out)[i] = static_cast<uint8_t>('A' + (i % 26));
            }
        }
        return get_random_result; 
    }
};

class MockClock : public neondoll::Clock {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init_result = 0;
    int deinit_result = 0;
    uint64_t fake_time = 1000;

    int init() override { 
        init_called = true; 
        return init_result; 
    }
    int deinit() override { 
        deinit_called = true; 
        return deinit_result; 
    }
    uint64_t now_ms() override { return fake_time; }
};

class MockNetworkAvailability : public neondoll::NetworkAvailability {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init_result = 0;
    int deinit_result = 0;
    int set_callback_result = -1;

    int init() override { 
        init_called = true; 
        return init_result; 
    }
    int deinit() override { 
        deinit_called = true; 
        return deinit_result; 
    }
    int set_callback(Callback cb, void* arg) override { 
        return set_callback_result; 
    }
};

class MockLogger : public neondoll::Logger {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init_result = 0;
    int deinit_result = 0;
    int log_result = 0;

    int init() override { 
        init_called = true; 
        return init_result; 
    }
    int deinit() override { 
        deinit_called = true; 
        return deinit_result; 
    }
    int log(int level, const char* tag, const char* msg) override { 
        (void)level; (void)tag; (void)msg;
        return log_result; 
    }
};

// Helper functions to reset specific mock types
void reset_mock(MockStorage& mock) {
    mock.init_called = false;
    mock.deinit_called = false;
    mock.init_result = 0;
    mock.deinit_result = 0;
    mock.read_result = -1;
    mock.write_result = -1;
    mock.last_read_size = 0;
    mock.last_read_bytes = -1;
    mock.last_write_size = 0;
}

void reset_mock(MockEntropySource& mock) {
    mock.init_called = false;
    mock.deinit_called = false;
    mock.init_result = 0;
    mock.deinit_result = 0;
    mock.get_random_result = -1;
}

void reset_mock(MockClock& mock) {
    mock.init_called = false;
    mock.deinit_called = false;
    mock.init_result = 0;
    mock.deinit_result = 0;
}

void reset_mock(MockNetworkAvailability& mock) {
    mock.init_called = false;
    mock.deinit_called = false;
    mock.init_result = 0;
    mock.deinit_result = 0;
    mock.set_callback_result = -1;
}

void reset_mock(MockLogger& mock) {
    mock.init_called = false;
    mock.deinit_called = false;
    mock.init_result = 0;
    mock.deinit_result = 0;
    mock.log_result = 0;
}

int test_success_case() {
    std::cout << "Testing success case..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == 0);
    assert(storage.init_called);
    assert(entropy.init_called);
    assert(clock.init_called);
    assert(network.init_called);
    assert(logger.init_called);

    neondoll_deinit(&ctx);
    assert(storage.deinit_called);
    assert(entropy.deinit_called);
    assert(clock.deinit_called);
    assert(network.deinit_called);
    assert(logger.deinit_called);

    std::cout << "Success case passed." << std::endl;
    return 0;
}

int test_storage_init_failure() {
    std::cout << "Testing storage init failure..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    storage.init_result = -5; // Simulate storage init failure

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == -5);
    assert(storage.init_called);
    assert(!entropy.init_called); // Should not be called
    assert(!clock.init_called);
    assert(!network.init_called);
    assert(!logger.init_called);

    // Storage should NOT be deinit'd on its own failure (never successfully initialized)
    assert(!storage.deinit_called);
    assert(!entropy.deinit_called);
    assert(!clock.deinit_called);
    assert(!network.deinit_called);
    assert(!logger.deinit_called);

    std::cout << "Storage init failure test passed." << std::endl;
    return 0;
}

int test_entropy_init_failure() {
    std::cout << "Testing entropy init failure..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    entropy.init_result = -3; // Simulate entropy init failure

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == -3);
    assert(storage.init_called);
    assert(storage.deinit_called); // Storage should be rolled back
    assert(entropy.init_called);
    assert(!clock.init_called);
    assert(!network.init_called);
    assert(!logger.init_called);

    assert(!entropy.deinit_called); // Entropy should not be deinit'd on its own failure

    std::cout << "Entropy init failure test passed." << std::endl;
    return 0;
}

int test_clock_init_failure() {
    std::cout << "Testing clock init failure..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    clock.init_result = -7; // Simulate clock init failure

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == -7);
    assert(storage.init_called);
    assert(storage.deinit_called); // Rolled back
    assert(entropy.init_called);
    assert(entropy.deinit_called); // Rolled back
    assert(clock.init_called);
    assert(!network.init_called);
    assert(!logger.init_called);

    assert(!clock.deinit_called); // Clock should not be deinit'd on its own failure

    std::cout << "Clock init failure test passed." << std::endl;
    return 0;
}

int test_network_init_failure() {
    std::cout << "Testing network init failure..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    network.init_result = -11; // Simulate network init failure

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == -11);
    assert(storage.init_called);
    assert(storage.deinit_called); // Rolled back
    assert(entropy.init_called);
    assert(entropy.deinit_called); // Rolled back
    assert(clock.init_called);
    assert(clock.deinit_called); // Rolled back
    assert(network.init_called);
    assert(!logger.init_called);

    assert(!network.deinit_called); // Network should not be deinit'd on its own failure

    std::cout << "Network init failure test passed." << std::endl;
    return 0;
}

int test_logger_init_failure() {
    std::cout << "Testing logger init failure..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    logger.init_result = -13; // Simulate logger init failure

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == -13);
    assert(storage.init_called);
    assert(storage.deinit_called); // Rolled back
    assert(entropy.init_called);
    assert(entropy.deinit_called); // Rolled back
    assert(clock.init_called);
    assert(clock.deinit_called); // Rolled back
    assert(network.init_called);
    assert(network.deinit_called); // Rolled back
    assert(logger.init_called);

    assert(!logger.deinit_called); // Logger should not be deinit'd on its own failure

    std::cout << "Logger init failure test passed." << std::endl;
    return 0;
}

int test_double_init_prevention() {
    std::cout << "Testing double init prevention..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    // First init should succeed
    assert(neondoll_init(&ctx) == 0);
    assert(storage.init_called);

    // Second init should fail
    assert(neondoll_init(&ctx) == -1); // Already initialized

    // Should not call init again on any platform
    assert(storage.init_called); // Still true from first call
    assert(!storage.deinit_called); // No deinit yet

    // Deinit should work
    neondoll_deinit(&ctx);
    assert(storage.deinit_called);

    // After deinit, init should work again
    reset_mock(storage);
    reset_mock(entropy);
    reset_mock(clock);
    reset_mock(network);
    reset_mock(logger);

    assert(neondoll_init(&ctx) == 0);
    assert(storage.init_called);
    
    // Clean up: deinit to leave system in clean state for next test
    neondoll_deinit(&ctx);

    std::cout << "Double init prevention test passed." << std::endl;
    return 0;
}

int test_binary_storage_contracts() {
    std::cout << "Testing binary storage contracts..." << std::endl;

    class BinaryStorageMock : public neondoll::Storage {
    public:
        bool init_called = false;
        bool deinit_called = false;

        int init() override { 
            init_called = true; 
            return 0; 
        }
        int deinit() override { 
            deinit_called = true; 
            return 0; 
        }

        // Test read contracts: exact byte counts, no null terminator assumptions
        std::ptrdiff_t read(const char* key, void* out, size_t max_size) override {
            (void)key; // unused
            if (max_size == 0) {
                return -2; // buffer too small (even 0 bytes is too small for our test data)
            }
            if (max_size < 5) {
                return -2; // buffer too small
            }
            // Exactly 5 bytes: 'h','e','l','l','o'
            const char* data = "hello";
            size_t data_len = 5;
            if (max_size < data_len) {
                return -2; // buffer too small
            }
            memcpy(out, data, data_len);
            return static_cast<std::ptrdiff_t>(data_len); // exact byte count
        }

        // Test write contracts
        int write(const char* key, const void* in, size_t size) override {
            (void)key; // unused
            if (size == 0) {
                return 0; // zero-length write should be allowed
            }
            // Accept any size, just don't crash
            return 0;
        }
    };

    BinaryStorageMock storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    assert(neondoll_init(&ctx) == 0);

    // Test read with insufficient buffer
    char small_buf[3];
    std::ptrdiff_t read_result = storage.read("test", small_buf, sizeof(small_buf));
    assert(read_result == -2); // buffer too small

    // Test read with exact buffer size
    char exact_buf[5];
    read_result = storage.read("test", exact_buf, sizeof(exact_buf));
    assert(read_result == 5); // exact byte count
    assert(memcmp(exact_buf, "hello", 5) == 0);

    // Test read with larger buffer
    char large_buf[10];
    read_result = storage.read("test", large_buf, sizeof(large_buf));
    assert(read_result == 5); // still only 5 bytes written
    assert(memcmp(large_buf, "hello", 5) == 0);
    // Bytes beyond 5 should be untouched (we didn't write to them)

    // Test write
    int write_result = storage.write("test", "data", 4);
    assert(write_result == 0); // success

    // Test zero-length write
    write_result = storage.write("test", nullptr, 0);
    assert(write_result == 0); // success

    neondoll_deinit(&ctx);

    std::cout << "Binary storage contracts test passed." << std::endl;
    return 0;
}

int test_runtime_object() {
    std::cout << "Testing Runtime object..." << std::endl;

    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    neondoll::Runtime runtime(ctx);

    // Initially not initialized
    assert(!runtime.is_initialized());

    // Initialize
    assert(runtime.init() == 0);
    assert(runtime.is_initialized());
    assert(storage.init_called);
    assert(entropy.init_called);
    assert(clock.init_called);
    assert(network.init_called);
    assert(logger.init_called);

    // Double init should fail
    assert(runtime.init() == -1); // already initialized

    // Deinitialize
    runtime.deinit();
    assert(!runtime.is_initialized());
    assert(storage.deinit_called);
    assert(entropy.deinit_called);
    assert(clock.deinit_called);
    assert(network.deinit_called);
    assert(logger.deinit_called);

    // After deinit, init should work again
    reset_mock(storage);
    reset_mock(entropy);
    reset_mock(clock);
    reset_mock(network);
    reset_mock(logger);

    assert(runtime.init() == 0);
    assert(runtime.is_initialized());
    assert(storage.init_called);

    std::cout << "Runtime object test passed." << std::endl;
    return 0;
}

int main() {
    std::cout << "Running comprehensive M0.2 host test..." << std::endl;

    int result = 0;
    result |= test_success_case();
    result |= test_storage_init_failure();
    result |= test_entropy_init_failure();
    result |= test_clock_init_failure();
    result |= test_network_init_failure();
    result |= test_logger_init_failure();
    result |= test_double_init_prevention();
    result |= test_binary_storage_contracts();
    result |= test_runtime_object();

    if (result == 0) {
        std::cout << "All tests passed." << std::endl;
    } else {
        std::cout << "Some tests failed." << std::endl;
    }

    return result;
}