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
    neondoll::deinit();

    // Assert that the functions were called
    assert(platform.init_called && "init() was not called");
    assert(platform.deinit_called && "deinit() was not called");
    // Note: The narrow interfaces are not called by the current neondoll::init/deinit
    // They are part of the platform interface for future use.
    // For M0, we only require that the platform interface exists and can be called.

    std::cout << "All tests passed.\n";
    return 0;
}