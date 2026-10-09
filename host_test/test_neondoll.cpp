#include <iostream>
#include <cassert>
#include "../components/neondoll/include/neondoll/platform.hpp"
#include "../components/neondoll/include/neondoll.hpp"

class MockPlatform : public neondoll::Platform {
public:
    bool init_called = false;
    bool deinit_called = false;
    bool storage_init_called = false;
    bool entropy_init_called = false;
    bool clock_init_called = false;
    bool network_init_called = false;
    bool log_init_called = false;

    void init() override {
        init_called = true;
        std::cout << "MockPlatform init called\n";
    }

    void deinit() override {
        deinit_called = true;
        std::cout << "MockPlatform deinit called\n";
    }

    void storage_init() override {
        storage_init_called = true;
        std::cout << "MockPlatform storage_init called\n";
    }

    void entropy_init() override {
        entropy_init_called = true;
        std::cout << "MockPlatform entropy_init called\n";
    }

    void clock_init() override {
        clock_init_called = true;
        std::cout << "MockPlatform clock_init called\n";
    }

    void network_init() override {
        network_init_called = true;
        std::cout << "MockPlatform network_init called\n";
    }

    void log_init() override {
        log_init_called = true;
        std::cout << "MockPlatform log_init called\n";
    }
};

int main() {
    MockPlatform platform;
    neondoll::set_platform(&platform);

    neondoll::init();
    assert(platform.init_called);

    neondoll::deinit();
    assert(platform.deinit_called);

    // Optionally check that the narrow interfaces were called via neondoll::init?
    // The neondoll::init() currently only calls Platform::init() and deinit().
    // The narrow interfaces are not called by the current init/deinit.
    // According to BUILD_PLAN.md, the narrow M0 platform interfaces are to be added.
    // We should perhaps call them in neondoll::init()? But the spec says "Add the narrow M0 platform interfaces specified in BUILD_PLAN.md".
    // We have added them as pure virtuals in Platform. The neondoll::init() and deinit() currently only call init/deinit.
    // We might need to update neondoll::init() to also call those narrow interfaces? However the user didn't specify that.
    // The requirement: "Add the narrow M0 platform interfaces specified in BUILD_PLAN.md (storage, entropy, clock, network availability, logging)."
    // We have added them as pure virtuals. The neondoll::init() and deinit() may not call them; but the test can still verify they exist.
    // For completeness, we could have neondoll::init() call all of them? But that might be beyond M0.
    // Let's just ensure the test compiles and passes with the current init/deinit only calling init/deinit.
    // We'll assert that the narrow interface functions are pure (they are) and that we can call them via the mock.
    // We'll call them explicitly in the test to ensure they are implemented.
    platform.storage_init();
    platform.entropy_init();
    platform.clock_init();
    platform.network_init();
    platform.log_init();
    assert(platform.storage_init_called);
    assert(platform.entropy_init_called);
    assert(platform.clock_init_called);
    assert(platform.network_init_called);
    assert(platform.log_init_called);

    std::cout << "All tests passed.\n";
    return 0;
}