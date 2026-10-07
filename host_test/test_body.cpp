#include <iostream>
#include <string>
#include <cassert>
#include "neondoll/identity.hpp"
#include "neondoll/body.hpp"

int main() {
    using namespace neondoll;

    // Test IdentityImpl
    IdentityImpl id;
    assert(id.body_id() == "");
    assert(!id.is_initialized());

    id.set_initialized(true);
    assert(id.body_id() == "placeholder-body-id");
    assert(id.is_initialized());

    // Test BodyImpl
    BodyImpl body;
    assert(body.init());
    auto identity = body.identity();
    assert(identity != nullptr);
    assert(identity->is_initialized());
    assert(identity->body_id() == "placeholder-body-id");

    std::cout << "All tests passed!" << std::endl;
    return 0;
}