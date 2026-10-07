#include <iostream>
#include <string>
#include <cassert>
#include <memory>
#include "neondoll/identity.hpp"
#include "neondoll/body.hpp"

int main() {
    using namespace neondoll;

    // Test IdentityImpl
    auto id = create_identity();
    assert(id->body_id() == 0);
    // Note: No is_initialized() method in M0 - identity is considered valid once created

    // Test BodyImpl
    BodyImpl body;
    assert(body.init());
    auto identity = body.identity();
    assert(identity != nullptr);
    assert(identity->body_id() == 0);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}