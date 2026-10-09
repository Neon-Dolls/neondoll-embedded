#include <neondoll/platform.hpp>
#include <cassert>
#include <cstring>
#include <iostream>

// Mock implementations for each interface
class MockStorage : public neondoll::Storage {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init() override { init_called = true; return 0; }
    int deinit() override { deinit_called = true; return 0; }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override { return -1; }
    int write(const char* key, const void* in, size_t size) override { return -1; }
};

class MockEntropySource : public neondoll::EntropySource {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init() override { init_called = true; return 0; }
    int deinit() override { deinit_called = true; return 0; }
    int get_random(void* out, size_t len) override { return -1; }
};

class MockClock : public neondoll::Clock {
public:
    bool init_called = false;
    bool deinit_called = false;
    uint64_t fake_time = 1000;
    int init() override { init_called = true; return 0; }
    int deinit() override { deinit_called = true; return 0; }
    uint64_t now_ms() override { return fake_time; }
};

class MockNetworkAvailability : public neondoll::NetworkAvailability {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init() override { init_called = true; return 0; }
    int deinit() override { deinit_called = true; return 0; }
    int set_callback(Callback cb, void* arg) override { return -1; }
};

class MockLogger : public neondoll::Logger {
public:
    bool init_called = false;
    bool deinit_called = false;
    int init() override { init_called = true; return 0; }
    int deinit() override { deinit_called = true; return 0; }
    int log(int level, const char* tag, const char* msg) override { 
        // For test, we just accept the call and return success.
        (void)level; (void)tag; (void)msg;
        return 0; 
    }
};

int main() {
    std::cout << "Running M0.2 host test..." << std::endl;

    // Create mock objects
    MockStorage storage;
    MockEntropySource entropy;
    MockClock clock;
    MockNetworkAvailability network;
    MockLogger logger;

    // Set up the platform context
    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    // Initialize NeonDoll with explicit context
    assert(neondoll_init(&ctx) == 0);
    assert(storage.init_called == true);
    assert(entropy.init_called == true);
    assert(clock.init_called == true);
    assert(network.init_called == true);
    assert(logger.init_called == true);

    // Deinitialize
    neondoll_deinit(&ctx);
    assert(storage.deinit_called == true);
    assert(entropy.deinit_called == true);
    assert(clock.deinit_called == true);
    assert(network.deinit_called == true);
    assert(logger.deinit_called == true);

    // Test that the narrow interface methods can be called (they return -1 or 0 as stubs)
    char key[] = "test";
    char value[32] = {0};
    assert(storage.read(key, value, sizeof(value)) == -1);
    assert(storage.write(key, value, sizeof(value)) == -1);

    uint8_t rand[16];
    assert(entropy.get_random(rand, sizeof(rand)) == -1);

    assert(clock.now_ms() == 1000);

    bool network_available = false;
    assert(network.set_callback([](void* arg, bool available) {
        bool* flag = static_cast<bool*>(arg);
        *flag = available;
    }, &network_available) == -1);

    assert(logger.log(0, "test", "hello") == 0);

    std::cout << "All tests passed." << std::endl;
    return 0;
}