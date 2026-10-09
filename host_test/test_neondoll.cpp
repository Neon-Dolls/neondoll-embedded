#include <iostream>
#include <cassert>
#include "../components/neondoll/include/neondoll/platform.hpp"
#include "../components/neondoll/include/neondoll.hpp"

class MockPlatform : public neondoll::Platform {
public:
    bool init_called = false;
    bool deinit_called = false;

    void init() override {
        init_called = true;
        std::cout << "MockPlatform init called\n";
    }

    void deinit() override {
        deinit_called = true;
        std::cout << "MockPlatform deinit called\n";
    }

    void storage_init() override {}
    void entropy_init() override {}
    void clock_init() override {}
    void network_init() override {}
    void log_init() override {}
};

int main() {
    auto* platform = new MockPlatform();
    neondoll::set_platform(platform);
    neondoll_init();
    neondoll_deinit();

    assert(platform->init_called && "init() was not called");
    assert(platform->deinit_called && "deinit() was not called");

    delete platform;
    return 0;
}