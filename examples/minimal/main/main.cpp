#include <iostream>
#include "neondoll/body.hpp"

int main() {
    neondoll::BodyImpl body;
    if (!body.init()) {
        std::cerr << "Failed to initialize Body" << std::endl;
        return 1;
    }
    auto id = body.identity();
    std::cout << "Body ID: " << id->body_id() << std::endl;
    // In a real ESP32 example, we would blink an LED here.
    // For example, using ESP-IDF's gpio module.
    // But for M0, we just initialize the Body.
    return 0;
}