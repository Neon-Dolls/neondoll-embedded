#include <neondoll/body.hpp>
#include <neondoll/identity.hpp>

namespace neondoll {

BodyImpl::BodyImpl() : identity_(create_identity()) {}

bool BodyImpl::init() {
    // For M0, we consider the Body initialized once we have an identity.
    // In a real implementation, this would load or create the durable identity.
    return true;
}

std::shared_ptr<Identity> BodyImpl::identity() const {
    return identity_;
}

} // namespace neondoll