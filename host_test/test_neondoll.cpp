#include <iostream>
#include "../components/neondoll/include/neondoll/platform.hpp"
#include "../components/neondoll/include/neondoll.hpp"

class MockPlatform : public neondoll::Platform {
public:
    bool init_called = false;
    bool deinit_called = false;

    void init() override {
        init_called = true;
        std::cout << "MockPlatform::init() called" << std::endl;
    }

    void deinit() override {
        deinit_called = true;
        std::cout << "MockPlatform::deinit() called" << std::endl;
    }
};

int main() {
    MockPlatform mock;
    neondoll::set_platform(&mock);

    neondoll::init();
    neondoll::deinit();

    if (mock.init_called && mock.deinit_called) {
        std::cout << "Host test PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "Host test FAILED" << std::endl;
        return 1;
    }
}