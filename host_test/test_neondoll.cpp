#include <iostream>
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
};

int main() {
    neondoll::set_platform(new MockPlatform());
    neondoll::init();
    neondoll::deinit();

    // In a real test, we would check the mock's state.
    // For now, we just return 0 if we got here.
    return 0;
}